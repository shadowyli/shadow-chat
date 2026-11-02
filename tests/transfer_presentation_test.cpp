#include "BeeApplication.h"
#include "ChatManager.h"
#include "Core.h"
#include "GuiChat.h"
#include "GuiChatItem.h"
#include "GuiChatMessage.h"
#include "GuiMain.h"
#include "Protocol.h"
#include "MessageManager.h"
#include "Random.h"
#include "Settings.h"
#include "ShadowChatTranscript.h"
#include "ShadowStyle.h"
#include "UserManager.h"
#include "IconManager.h"
#include <QtTest>

#define CHECK(value) do { if(!(value)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#value); return 1; } } while(false)

class TransferChatProbe : public GuiChat
{
public:
  using GuiChat::applyTransferIndicators;
  using GuiChat::modernHistoryHtml;
  using GuiChat::chatMessageToText;
};

class TransferCoreProbe : public Core
{
public:
  using Core::sendMessageToChat;
  using Core::checkFileTransferMessage;
  using Core::showFilesUploadPreviewInChat;
};

static QStringList indicatorIds( QTextDocument* document )
{
  QStringList ids;
  for(QTextBlock block=document->begin();block.isValid();block=block.next())
    for(QTextBlock::iterator iterator=block.begin();!iterator.atEnd();++iterator)
    {
      const QTextFragment fragment=iterator.fragment();
      if(!fragment.charFormat().isImageFormat())continue;
      const QUrl source(fragment.charFormat().toImageFormat().name());
      if(source.scheme()=="shadowtransfer")ids.append(source.path());
    }
  return ids;
}

int main( int argc, char** argv )
{
  std::setvbuf(stdout,Q_NULLPTR,_IONBF,0);
  if(qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))qputenv("QT_QPA_PLATFORM","offscreen");
  BeeApplication app(argc,argv);
  app.setFont(ShadowStyle::applicationFont());
  app.setStyleSheet(ShadowStyle::applicationStyleSheet());
  Random::init();
  QTemporaryDir temporary;
  CHECK(temporary.isValid());
  Settings& settings=Settings::instance();
  settings.setDefaultFolders();
  settings.createLocalUser("transfer-presentation-fixture");
  settings.setAccountDataFolder(temporary.path());
  settings.load();
  TransferCoreProbe core;
  User peer(97011);
  peer.setName("Offline fixture");
  peer.setHash("transfer-presentation-peer");
  peer.setStatus(User::Offline);
  UserManager::instance().setUser(peer);
  Group group;
  group.setId(99011);
  group.setName(peer.name());
  group.setChatType(Group::PrivateChat);
  group.addUser(ID_LOCAL_USER);
  group.addUser(peer.id());
  Chat initial;
  initial.setGroup(group);
  ChatManager::instance().setChat(initial);
  int deliveryNotices=0;
  QObject::connect(&core,&Core::newChatMessage,&app,[&](const Chat&,const ChatMessage& message) {
    if(message.type()==ChatMessage::Other)++deliveryNotices;
  });
  const int queuedTextBefore=MessageManager::instance().countMessagesToSendInChatId(initial.id());
  CHECK(core.sendMessageToChat(initial,Message(Message::Chat,99090,"offline text retained"))==0);
  CHECK(MessageManager::instance().countMessagesToSendInChatId(initial.id())==queuedTextBefore+1);
  CHECK(deliveryNotices==0);
  QTreeWidget list;
  GuiChatItem item(&list);
  CHECK(item.updateItem(initial));
  const QImage offline=item.icon(0).pixmap(48,48).toImage();
  CHECK(!offline.isNull());
  for(int row=0;row<offline.height();++row)
    for(int column=0;column<offline.width();++column)
    {
      const QColor pixel=offline.pixelColor(column,row);
      CHECK(pixel.red()==pixel.green()&&pixel.green()==pixel.blue());
    }
  peer.setStatus(User::Online);
  UserManager::instance().setUser(peer);
  CHECK(item.updateItem(initial));
  CHECK(item.icon(0).pixmap(48,48).toImage()!=offline);
  peer.setStatus(User::Offline);
  UserManager::instance().setUser(peer);

  QImage image(320,180,QImage::Format_RGB32);
  image.fill(QColor("#79A9E8"));
  const QString imagePath=temporary.filePath("screenshot.png");
  CHECK(image.save(imagePath));
  const QString filePath=temporary.filePath("material.txt");
  QFile sourceFile(filePath);
  CHECK(sourceFile.open(QIODevice::WriteOnly));
  sourceFile.write("synthetic attachment");
  sourceFile.close();
  CHECK(core.sendFilesFromChat(initial.id(),QStringList{imagePath,filePath})==2);
  const QVariantList queued=core.outgoingFileTransferStates(initial.id());
  CHECK(queued.size()==2);
  for(const QVariant& task:queued)CHECK(task.toMap().value("state")=="pending");

  QToolBar toolbar;
  TransferChatProbe chat;
  chat.setupToolBar(&toolbar);
  chat.setModernMode(true);
  chat.resize(940,720);
  CHECK(chat.setChat(ChatManager::instance().chat(initial.id())));
  chat.show();
  QTest::qWait(120);
  auto* transcript=chat.findChild<ShadowChatTranscript*>("GuiChatViewer");
  CHECK(transcript);
  const auto saved=[](ChatMessage::Type type,const QString& body) {
    QJsonObject metadata;
    metadata.insert("type",int(type));
    return QString("<!--shadow-record:%1-->%2<br>").arg(QString::fromLatin1(QJsonDocument(metadata).toJson(QJsonDocument::Compact).toBase64()),body);
  };
  const QString fileUrl=QUrl::fromLocalFile(filePath).toString();
  const QString archived=saved(ChatMessage::FileTransfer,QStringLiteral("等待发送 old screenshot"))+
    saved(ChatMessage::Chat,QStringLiteral("我说：等待发送也要正确显示"))+
    saved(ChatMessage::FileTransfer,QString("<!--shadow-attachment--><a href=\"%1\">material.txt</a>").arg(fileUrl));
  const QString visibleHistory=chat.modernHistoryHtml(archived);
  CHECK(!visibleHistory.contains("old screenshot"));
  CHECK(visibleHistory.contains(QStringLiteral("我说：等待发送也要正确显示"))&&visibleHistory.contains("material.txt"));
  const QString notice=IconManager::instance().toHtml("unsent-message.png","*m*")+QStringLiteral("消息即将发送给 Offline fixture.");
  const ChatMessage queuedNotice(ID_SYSTEM_MESSAGE,Protocol::instance().systemMessage(notice),ChatMessage::Other,false);
  CHECK(chat.chatMessageToText(queuedNotice).isEmpty());
  CHECK(chat.modernHistoryHtml(saved(ChatMessage::Other,notice)).isEmpty());
  CHECK(!chat.modernHistoryHtml(saved(ChatMessage::Chat,QStringLiteral("我说：消息即将发送给你"))).isEmpty());
  const ChatMessage failedTransfer(ID_SYSTEM_MESSAGE,Protocol::instance().systemMessage(
    QStringLiteral("<!--shadow-transfer-error-->接收方已经离线，文件发送失败。")),ChatMessage::FileTransfer,false);
  CHECK(chat.chatMessageToText(failedTransfer).contains(QStringLiteral("文件发送失败")));
  CHECK(chat.modernHistoryHtml(saved(ChatMessage::FileTransfer,QStringLiteral("无法开始文件传输，请重试。")))
    .contains(QStringLiteral("无法开始文件传输")));
  const ChatMessage normalProgress(ID_SYSTEM_MESSAGE,Protocol::instance().systemMessage(
    QStringLiteral("Downloading screenshot.png from Offline fixture.")),ChatMessage::FileTransfer,false);
  CHECK(chat.chatMessageToText(normalProgress).isEmpty());
  const QString imageUrl=QUrl::fromLocalFile(imagePath).toString();
  QUrl folderUrl=QUrl::fromLocalFile(imagePath);
  folderUrl.setScheme(FileInfo::urlSchemeShowFileInFolder());
  const QString archivedImage=saved(ChatMessage::ImagePreview,QString("<a href=\"%1\"><img src=\"%1\" width=\"160\"></a>").arg(imageUrl));
  const QString redundantCard=saved(ChatMessage::FileTransfer,QString("Offline fixture sent <a href=\"%1\">duplicate image card</a>. <a href=\"%2\">folder</a>").arg(imageUrl,folderUrl.toString()));
  const QString imageHistory=chat.modernHistoryHtml(archivedImage+redundantCard);
  CHECK(imageHistory.contains("<img")&&!imageHistory.contains("duplicate image card"));
  const QStringList ids=indicatorIds(transcript->document());
  CHECK(ids.size()==2&&transcript->bubbleCount()==2);
  CHECK(transcript->toPlainText().contains("material.txt"));
  CHECK(!transcript->toPlainText().contains(QStringLiteral("等待发送")));
  for(const QString& id:ids)CHECK(transcript->transferIndicatorState(id)==ShadowChatTranscript::QueuedIndicator);
  QTimer* animation=transcript->findChild<QTimer*>("ShadowTransferAnimationTimer");
  QTimer* refresh=chat.findChild<QTimer*>("ShadowTransferRefreshTimer");
  CHECK(animation&&refresh&&!animation->isActive());
  QToolButton* screenshot=Q_NULLPTR;
  for(QToolButton* button:chat.findChildren<QToolButton*>())
    if(button->accessibleName()==QStringLiteral("截图"))screenshot=button;
  CHECK(screenshot&&screenshot->icon().pixmap(24,24).toImage()==ShadowStyle::actionIcon("scissors").pixmap(24,24).toImage());

  QVariantList active;
  for(const QVariant& task:queued)
  {
    QVariantMap state=task.toMap();
    state["state"]="offered";
    active.append(state);
  }
  chat.applyTransferIndicators(active);
  refresh->stop();
  CHECK(animation->isActive());
  for(const QString& id:ids)CHECK(transcript->transferIndicatorState(id)==ShadowChatTranscript::SendingIndicator);
  QTextCursor selection=transcript->document()->find("material.txt");
  transcript->setTextCursor(selection);
  const int scroll=transcript->verticalScrollBar()->value();
  const QImage firstFrame=transcript->viewport()->grab().toImage();
  QTest::qWait(170);
  CHECK(transcript->viewport()->grab().toImage()!=firstFrame);
  CHECK(transcript->textCursor().selectedText()=="material.txt"&&transcript->verticalScrollBar()->value()==scroll);
  CHECK(chat.grab().save(QDir::currentPath()+"/transfer-sending-inline.png"));
  chat.hide();
  CHECK(!animation->isActive());
  chat.show();
  CHECK(animation->isActive());

  QVariantMap failure=active.first().toMap();
  failure["state"]="failed";
  failure["error"]="synthetic refused transfer";
  active[0]=failure;
  chat.applyTransferIndicators(active);
  refresh->stop();
  bool failed=false;
  for(const QString& id:ids)failed=failed||transcript->transferIndicatorState(id)==ShadowChatTranscript::FailedIndicator;
  CHECK(failed);
  QVariantList completed;
  for(const QVariant& task:queued)
  {
    QVariantMap state=task.toMap();
    state["state"]="completed";
    completed.append(state);
  }
  chat.applyTransferIndicators(completed);
  CHECK(!animation->isActive()&&!refresh->isActive());
  for(const QString& id:ids)CHECK(transcript->transferIndicatorState(id)==ShadowChatTranscript::NoIndicator);
  CHECK(core.outgoingFileTransferStates(initial.id()).size()==2);

  QLabel* feedback=chat.findChild<QLabel*>("ShadowSendFeedback");
  QTimer* feedbackTimer=chat.findChild<QTimer*>("ShadowSendFeedbackTimer");
  CHECK(feedback&&feedbackTimer);
  chat.setFileSendResult(true,QStringLiteral("文件已加入发送队列"));
  CHECK(feedback->isHidden()&&feedback->text().isEmpty()&&!feedbackTimer->isActive());
  CHECK(core.outgoingFileTransferStates(initial.id()).size()==2);
  chat.setMessageSendResult("accepted message",true,QStringLiteral("消息已提交发送，等待对方接收。"));
  CHECK(feedback->isHidden()&&feedback->text().isEmpty()&&!feedbackTimer->isActive());
  chat.showSendFeedback("other transient feedback",false);
  CHECK(feedback->isVisible()&&feedbackTimer->isActive()&&feedbackTimer->interval()==5000);
  feedbackTimer->start(30);
  chat.setFileSendResult(false,"failure must remain");
  QTest::qWait(80);
  CHECK(feedback->isVisible()&&feedback->text()=="failure must remain"&&!feedbackTimer->isActive());
  Chat another=initial;
  Group otherGroup=group;
  otherGroup.setId(99012);
  otherGroup.setName("Other fixture chat");
  another.setGroup(otherGroup);
  ChatManager::instance().setChat(another);
  CHECK(chat.setChat(another)&&feedback->isHidden());
  CHECK(chat.setChat(ChatManager::instance().chat(initial.id()))&&feedback->text()=="failure must remain");
  chat.setFileSendResult(true,"new transient notice");
  CHECK(chat.setChat(another)&&feedback->isHidden());
  CHECK(chat.setChat(ChatManager::instance().chat(initial.id()))&&feedback->isHidden());
  settings.setChatMessagesToShow(-1);
  const Chat outgoing=ChatManager::instance().chat(initial.id());
  QMap<QString,QString> archive;
  archive.insert(outgoing.name(),GuiChatMessage::chatToHtml(outgoing,false,false,true,true,false,true));
  ChatManager::instance().addSavedChats(archive);
  Chat restored;
  restored.setGroup(group);
  ChatManager::instance().setChat(restored);
  CHECK(chat.setChat(restored));
  QTest::qWait(80);
  const QStringList restoredIds=indicatorIds(transcript->document());
  CHECK(restoredIds.size()==2&&transcript->bubbleCount()==2);
  CHECK(!transcript->toPlainText().contains(QStringLiteral("等待发送")));
  for(const QString& id:restoredIds)CHECK(transcript->transferIndicatorState(id)==ShadowChatTranscript::QueuedIndicator);
  core.cancelPendingFileTransfers(ID_INVALID,initial.id());
  chat.refreshTransferIndicators();
  for(const QString& id:restoredIds)CHECK(transcript->transferIndicatorState(id)==ShadowChatTranscript::NoIndicator);
  CHECK(core.outgoingFileTransferStates(initial.id()).isEmpty());
  Group receivedGroup=group;
  receivedGroup.setId(99013);
  receivedGroup.setPrivateId("received-image-without-companion");
  Chat received;
  received.setGroup(receivedGroup);
  ChatManager::instance().setChat(received);
  FileInfo receivedImage(99101,FileInfo::Download);
  receivedImage.setNameAndSuffix("screenshot.png");
  receivedImage.setPath(imagePath);
  receivedImage.setSize(QFileInfo(imagePath).size());
  receivedImage.setChatPrivateId(receivedGroup.privateId());
  core.checkFileTransferMessage(99101,peer.id(),receivedImage,"completed",FileTransferPeer::Completed);
  const Chat afterImage=ChatManager::instance().chat(received.id());
  int imageMessages=0,fileMessages=0;
  for(const ChatMessage& message:afterImage.messages())
  {
    if(message.isImagePreview())++imageMessages;
    if(message.isFileTransfer())++fileMessages;
  }
  CHECK(imageMessages==1&&fileMessages==0);
  FileInfo receivedFile(99102,FileInfo::Download);
  receivedFile.setNameAndSuffix("material.txt");
  receivedFile.setPath(filePath);
  receivedFile.setSize(QFileInfo(filePath).size());
  receivedFile.setChatPrivateId(receivedGroup.privateId());
  core.checkFileTransferMessage(99102,peer.id(),receivedFile,"completed",FileTransferPeer::Completed);
  fileMessages=0;
  for(const ChatMessage& message:ChatManager::instance().chat(received.id()).messages())
    if(message.isFileTransfer())++fileMessages;
  CHECK(fileMessages==1);
  const QString stickerFolder=temporary.filePath("stickers");
  CHECK(QDir().mkpath(stickerFolder));
  const QString stickerPath=QDir(stickerFolder).filePath(QString(64,QLatin1Char('a'))+".gif");
  const QByteArray gifBytes=QByteArray::fromBase64(
    "R0lGODlhGAAMAIEAAP8AAAAAAAAAAAAAACH/C05FVFNDQVBFMi4wAwEAAAAh+QQACgAAACwAAAAAGAAMAAAIHgABCBxIsKDBgwgTKlzIsKHDhxAjSpxIsaLFiwwDAgAh+QQBCgABACwAAAAAGAAMAIEAAP8AAAAAAAAAAAAIHgABCBxIsKDBgwgTKlzIsKHDhxAjSpxIsaLFiwwDAgA7");
  QFile gifFile(stickerPath);
  CHECK(gifFile.open(QIODevice::WriteOnly)&&gifFile.write(gifBytes)==gifBytes.size());
  gifFile.close();
  CHECK(core.showFilesUploadPreviewInChat(received,QStringList{stickerPath}));
  const ChatMessage sentSticker=ChatManager::instance().chat(received.id()).messages().last();
  CHECK(sentSticker.isImagePreview()&&sentSticker.message().contains("<!--shadow-sticker-->"));
  CHECK(sentSticker.message().contains(QUrl::fromLocalFile(stickerPath).toString()));
  FileInfo receivedSticker(99103,FileInfo::Download);
  receivedSticker.setNameAndSuffix("received-animation.gif");
  receivedSticker.setPath(stickerPath);
  receivedSticker.setSize(gifBytes.size());
  receivedSticker.setMimeType("image/gif;shadow-sticker=1");
  receivedSticker.setChatPrivateId(receivedGroup.privateId());
  const Message wireSticker=Protocol::instance().fileInfoToMessage(receivedSticker,95);
  FileInfo decodedSticker=Protocol::instance().fileInfoFromMessage(wireSticker,95);
  CHECK(decodedSticker.mimeType()==receivedSticker.mimeType());
  CHECK(decodedSticker.name()==receivedSticker.name());
  decodedSticker.setPath(stickerPath);
  core.checkFileTransferMessage(99103,peer.id(),decodedSticker,"completed",FileTransferPeer::Completed);
  const ChatMessage receivedAnimation=ChatManager::instance().chat(received.id()).messages().last();
  CHECK(receivedAnimation.isImagePreview()&&receivedAnimation.message().contains("<!--shadow-sticker-->"));
  CHECK(receivedAnimation.message().contains(QUrl::fromLocalFile(stickerPath).toString()));
  CHECK(!receivedAnimation.message().contains(".png"));
  std::puts("PASS core presentation: original GIF preview on send/receive, sticker wire metadata, hidden success notices, real queues and failure retention");
  GuiMain window;
  window.show();
  QTest::qWait(80);
  CHECK(window.findChild<QWidget*>("ShadowNetworkStrip")->isHidden());
  std::puts("PASS transfer presentation: offline avatars grayscale/online restore, hidden connection strip, scissors action, real offline queue and file/image bubbles, restored history/queue correlation and cancellation, no process logs or accepted-send banners, no duplicate image card, active animation/hidden stop, terminal states, selection preserved, failure retention and no stale chat notice");
  return 0;
}
