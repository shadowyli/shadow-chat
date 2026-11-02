#include "BeeApplication.h"
#include "ChatManager.h"
#include "Core.h"
#include "GuiChat.h"
#include "GuiChatItem.h"
#include "GuiChatMessage.h"
#include "GuiMain.h"
#include "Protocol.h"
#include "Random.h"
#include "Settings.h"
#include "ShadowChatTranscript.h"
#include "ShadowStyle.h"
#include "UserManager.h"
#include <QtTest>

#define CHECK(value) do { if(!(value)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#value); return 1; } } while(false)

class TransferChatProbe : public GuiChat
{
public:
  using GuiChat::applyTransferIndicators;
  using GuiChat::modernHistoryHtml;
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
  qputenv("QT_QPA_PLATFORM","offscreen");
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
  Core core;
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
  CHECK(feedback->isVisible()&&feedbackTimer->isActive()&&feedbackTimer->interval()==5000);
  QTest::qWait(5400);
  CHECK(feedback->isHidden()&&feedback->text().isEmpty());
  CHECK(core.outgoingFileTransferStates(initial.id()).size()==2);
  chat.setFileSendResult(true,"old success");
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
  GuiMain window;
  window.show();
  QTest::qWait(80);
  CHECK(window.findChild<QWidget*>("ShadowNetworkStrip")->isHidden());
  std::puts("PASS transfer presentation: offline avatars grayscale/online restore, hidden connection strip, scissors action, real offline queue and file/image bubbles, restored history/queue correlation and cancellation, no process logs, active animation/hidden stop, terminal states, selection preserved, 5s notice expiry, failure retention and no stale chat notice");
  return 0;
}
