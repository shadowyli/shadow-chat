#include "Config.h"
#include "BeeApplication.h"
#define private public
#include "Settings.h"
#undef private
#include "Core.h"
#include "ChatManager.h"
#include "MessageManager.h"
#include "UserManager.h"
#include "Protocol.h"
#include "Random.h"
#include "ShadowTransferQueue.h"
#include <cstdio>

class UpdateCoreProbe : public Core
{
public:
  using Core::parseMessage;
  using Core::sendMessageToChat;
  using Core::newPeerFound;
};

static QByteArray readFile(const QString& path)
{
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

int main(int argc, char** argv)
{
  BeeApplication application(argc, argv);
  QTemporaryDir temporary;
  if(!temporary.isValid())
    return 1;
  int failures = 0;
  auto check = [&](bool passed, const char* label) {
    std::printf("%s %s\n", passed ? "PASS" : "FAIL", label);
    if(!passed)
      ++failures;
  };
  Random::init();
  Settings& settings = Settings::instance();
  settings.setDefaultFolders();
  settings.createLocalUser("update-fixture");
  settings.setAccountDataFolder(temporary.path());
  settings.load();
  settings.setDownloadDirectory(temporary.path());
  settings.setResumeFileTransfer(false);
  settings.m_useHive = false;
  settings.m_useMulticastDns = false;
  settings.m_defaultListenerPort = 0;
  settings.m_defaultBroadcastPort = 0;
  settings.m_defaultFileTransferPort = 0;
  settings.m_broadcastOnlyToHostsIni = true;
  settings.m_broadcastAddressesInFileHosts.clear();
  settings.m_broadcastToLocalSubnetAlways = false;
  settings.m_disableMulticast = true;
  settings.m_broadcastToOfflineUsers = false;
  settings.setAskChangeUserAtStartup(false);
  MessageManager::instance().loadSavedMessagesAuthCode();
  Protocol::instance();
  application.init();
  UpdateCoreProbe core;
  core.init();
  check(core.start() && core.isConnected(), "real Core listeners start with discovery disabled");
  User peer;
  peer.setId(99);
  peer.setName("fixture-recipient");
  peer.setHash("update-fixture-peer");
  peer.setAccountName("fixture@example.test");
  peer.setStatus(User::Offline);
  UserManager::instance().setUser(peer);
  core.createPrivateChat(peer);
  const Chat chat = ChatManager::instance().privateChatForUser(peer.id());
  check(chat.isValid(), "fixture private chat exists");
  const Message pending = Protocol::instance().chatMessage(chat, "existing-unsent-fixture");
  MessageManager::instance().addMessageToSend(peer.id(), chat.id(), pending);
  const int pendingBefore = MessageManager::instance().countMessagesToSendToUserId(peer.id());
  check(pendingBefore == 1, "fixture message queued before blocking");
  const QString material = temporary.filePath("retained-material.txt");
  QFile source(material);
  check(source.open(QIODevice::WriteOnly), "fixture material opens");
  source.write("original-material");
  source.close();
  ShadowTransferQueue queue;
  QString error;
  check(queue.open(temporary.path()) && queue.enqueue(peer.hash(), chat.privateId(), false, material, &error), "fixture transfer queued");
  const QString taskId = queue.tasks().first().id;
  int delivered = 0;
  QObject::connect(&core, &Core::newChatMessage, [&](const Chat&, const ChatMessage& message) {
    if(message.message().contains("blocked-fixture"))
      ++delivered;
  });
  QMetaObject::invokeMethod(&core, [&core, chat]() {
    core.sendChatMessage(chat.id(), "blocked-fixture-queued-send", false, true, false);
  }, Qt::QueuedConnection);
  QMetaObject::invokeMethod(&core, [&core, peer, pending]() {
    core.parseMessage(peer, pending);
  }, Qt::QueuedConnection);
  core.blockForUpdate();
  check(!core.isConnected() && !core.isFileTransferActive(), "block stops chat and transfer listeners");
  check(!settings.resumeFileTransfer(), "temporary pause preserves original resume preference");
  check(!core.start(), "start rejected after blocking");
  core.restart();
  check(!core.isConnected(), "restart rejected after blocking");
  check(core.sendChatMessage(chat.id(), "blocked-fixture-direct-send", false, true, false) == 0, "direct text send rejected");
  check(core.sendMessageToChat(chat, pending) == 0, "internal send cannot append offline message");
  check(core.sendFilesFromChat(chat.id(), QStringList() << material) == 0, "file send rejected");
  check(!core.sendFile(peer.id(), material, "", false, chat.id()), "direct file send rejected");
  check(!core.startFileTransferServer(), "file listener restart rejected");
  FileInfo file(1, FileInfo::Download);
  file.setName("fixture.txt");
  file.setPath(temporary.filePath("received.txt"));
  check(!core.downloadFile(peer.id(), file, false), "download rejected");
  check(!core.resumeFileTransfer(peer.id(), file), "transfer resume rejected");
  check(!core.sendVoiceMessageToChat(chat.id(), material, 1000), "queued voice send rejected before creating chat records");
  const int recordsBefore = ChatManager::instance().chat(chat.id()).messages().size();
  const Message incoming = Protocol::instance().chatMessage(chat, "blocked-fixture-incoming");
  core.parseMessage(peer, incoming);
  core.parseMessage(peer.id(), incoming);
  core.newPeerFound(QHostAddress::LocalHost, 1);
  core.processPendingFileTransfers();
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
  check(delivered == 0 && ChatManager::instance().chat(chat.id()).messages().size() == recordsBefore, "direct and queued receive/send leave chat unchanged");
  check(MessageManager::instance().countMessagesToSendToUserId(peer.id()) == pendingBefore, "existing unsent message retained without new sends");
  const QByteArray savedMessages = readFile(settings.unsentMessagesFilePath());
  check(!savedMessages.isEmpty(), "block persists unsent messages on disk");
  QDataStream savedStream(savedMessages);
  savedStream.setVersion(settings.dataStreamVersion(false));
  QStringList header;
  qint32 savedCount = 0;
  QString savedRecord;
  savedStream >> header >> savedCount >> savedRecord;
  const MessageRecord restoredMessage = Protocol::instance().loadMessageRecord(settings.simpleDecrypt(savedRecord));
  check(savedStream.status() == QDataStream::Ok && savedCount == 1 &&
        restoredMessage.isValid() && restoredMessage.message().text() == pending.text(),
        "saved payload retains original unsent content");
  ShadowTransferQueue restored;
  check(restored.open(temporary.path()) && restored.tasks().size() == 1 &&
        restored.tasks().first().id == taskId && restored.tasks().first().state == "pending", "queued transfer survives block");
  check(readFile(material) == "original-material", "source material preserved byte for byte");
  core.blockForUpdate();
  check(readFile(settings.unsentMessagesFilePath()) == savedMessages, "repeated block preserves saved message file");
  application.cleanUp();
  return failures ? 1 : 0;
}
