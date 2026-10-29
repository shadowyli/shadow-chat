#include "BeeApplication.h"
#include "Settings.h"
#include "Core.h"
#include "Protocol.h"
#include "UserManager.h"
#include "ChatManager.h"
#include "MessageManager.h"
#include "GuiFloatingChat.h"
#include "GuiUserList.h"
#include "Random.h"
#include "ShadowPeerAvatars.h"
#include "BeeUtils.h"
#include <QProcess>
#include <QTemporaryDir>
#include <cstdio>
#define CHECK(value) do{if(!(value)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#value);return 1;}}while(false)
class CoreProbe:public Core {public:using Core::parseUserMessage;using Core::saveUsersAndGroups;using Core::loadUsersAndGroups;};
static QPixmap photo(const QColor& color){QPixmap pixmap(64,64);pixmap.fill(color);return pixmap;}
static QString avatarUrl(QTextDocument* document,VNumber userId)
{
  const QString marker=QString("chat-avatar-%1-").arg(userId);
  for(QTextBlock block=document->begin();block.isValid();block=block.next())for(QTextBlock::iterator it=block.begin();!it.atEnd();++it){const QTextFragment fragment=it.fragment();if(fragment.isValid() && fragment.charFormat().isImageFormat()){const QString url=fragment.charFormat().toImageFormat().name();if(url.contains(marker))return url;}}
  return QString();
}
static bool centerIs(QTextDocument* document,const QString& url,const QColor& expected)
{
  const QVariant resource=document->resource(QTextDocument::ImageResource,QUrl(url));QImage image=resource.value<QImage>();if(image.isNull())image=resource.value<QPixmap>().toImage();
  return !image.isNull() && QColor(image.pixel(image.width()/2,image.height()/2)).rgb()==expected.rgb();
}
static GuiUserItem* peerRow(QTreeWidget* tree,VNumber peerId)
{
  QTreeWidgetItemIterator it(tree);while(*it){GuiUserItem* item=dynamic_cast<GuiUserItem*>(*it);if(item && !item->isWorkgroup() && item->userId()==peerId)return item;++it;}return nullptr;
}
static int reload(const QString& folder,bool cleared)
{
  Settings& settings=Settings::instance();settings.setDefaultFolders();settings.createLocalUser("fresh-process");settings.setAccountDataFolder(folder);settings.load();settings.setRemoveInactiveUsers(false);Random::init();
  CHECK(settings.localUser().vCard().nickName()==QString::fromUtf8("李颖"));CHECK(settings.localUser().vCard().photo().toImage().pixelColor(32,32)==QColor(Qt::green));
  CoreProbe core;core.loadUsersAndGroups();User peer=UserManager::instance().findUserByHash("peer-avatar-identity");CHECK(peer.isValid());CHECK(peer.accountName()=="peer@example.test");CHECK(peer.name()==QString::fromUtf8("影子"));CHECK(!peer.isLocal());
  if(cleared)CHECK(peer.vCard().photo().isNull());else CHECK(peer.vCard().photo().toImage().pixelColor(32,32)==QColor(Qt::yellow));
  peer.setStatus(User::Online);UserManager::instance().setUser(peer);User local=settings.localUser();local.setStatus(User::Online);settings.setLocalUser(local);
  Group group;group.setId(7011);group.setName("restart-avatar");group.setChatType(Group::PrivateChat);group.addUser(ID_LOCAL_USER);group.addUser(peer.id());Chat chat;chat.setGroup(group);chat.addMessage(ChatMessage(ID_LOCAL_USER,Message(Message::Chat,7101,"restart-local"),ChatMessage::Chat,true));chat.addMessage(ChatMessage(peer.id(),Message(Message::Chat,7102,"restart-peer"),ChatMessage::Chat,true));ChatManager::instance().setChat(chat);
  GuiFloatingChat window;window.setEmbeddedMode(true);window.guiChat()->setModernMode(true);CHECK(window.setChat(chat));QTextBrowser* viewer=window.findChild<QTextBrowser*>("GuiChatViewer");CHECK(viewer);CHECK(centerIs(viewer->document(),avatarUrl(viewer->document(),ID_LOCAL_USER),Qt::green));if(!cleared)CHECK(centerIs(viewer->document(),avatarUrl(viewer->document(),peer.id()),Qt::yellow));CHECK(avatarUrl(viewer->document(),ID_LOCAL_USER)!=avatarUrl(viewer->document(),peer.id()));
  QTemporaryDir another;settings.setAccountDataFolder(another.path());User stranger=peer;VCard blank=stranger.vCard();blank.setPhoto(QPixmap());stranger.setVCard(blank);ShadowPeerAvatars::restore(&stranger);CHECK(stranger.vCard().photo().isNull());
  QSettings corrupt(ShadowPeerAvatars::path(),QSettings::IniFormat);corrupt.setValue(ShadowPeerAvatars::key(stranger),QByteArray("malformedPNG").toBase64());corrupt.sync();ShadowPeerAvatars::restore(&stranger);CHECK(stranger.vCard().photo().isNull());CHECK(ShadowPeerAvatars::decode(QByteArray(2*1024*1024+1,'a')).isNull());
  std::puts("PASS fresh-process settings/user reload, distinct local/peer chat photos, cleared-photo persistence, account isolation and corrupt/oversize rejection");return 0;
}
static bool childReload(const QString& folder,bool cleared)
{
  QProcess child;child.setProcessChannelMode(QProcess::ForwardedChannels);child.start(QCoreApplication::applicationFilePath(),QStringList()<<"--reload"<<folder<<(cleared?"clear":"photo"));return child.waitForFinished(30000)&&child.exitStatus()==QProcess::NormalExit&&child.exitCode()==0;
}
int main(int argc,char** argv)
{
  qputenv("QT_QPA_PLATFORM","offscreen");BeeApplication app(argc,argv);QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");if(app.arguments().value(1)=="--reload")return reload(app.arguments().value(2),app.arguments().value(3)=="clear");QTemporaryDir temporary;CHECK(temporary.isValid());
  Settings& settings=Settings::instance();settings.setDefaultFolders();settings.createLocalUser(QString::fromUtf8("李颖"));settings.setAccountDataFolder(temporary.path());settings.load();settings.setShowUserPhoto(true);settings.setShowUsersInWorkgroups(false);settings.setShowOnlyOnlineUsers(false);Random::init();MessageManager::instance().loadSavedMessagesAuthCode();
  User local=settings.localUser();local.setName(QString::fromUtf8("李颖"));local.setAccountName("self@example.test");local.setStatus(User::Online);VCard own=local.vCard();own.setNickName(QString::fromUtf8("李颖"));own.setPhoto(photo(Qt::red));local.setVCard(own);settings.setLocalUser(local);
  User peer(3001);peer.setName(QString::fromUtf8("影子"));peer.setAccountName("peer@example.test");peer.setHash("peer-avatar-identity");peer.setStatus(User::Online);peer.setProtocolVersion(settings.protocolVersion());VCard other=peer.vCard();other.setNickName(QString::fromUtf8("影子"));other.setPhoto(photo(Qt::blue));peer.setVCard(other);UserManager::instance().setUser(peer);
  User offlinePeer=peer;offlinePeer.setStatus(User::Offline);
  const QImage onlinePhoto=Bee::avatarForUser(peer,QSize(40,40),true).toImage();
  CHECK(onlinePhoto==Bee::avatarForUser(offlinePeer,QSize(40,40),true).toImage());CHECK(onlinePhoto.pixelColor(20,0)==QColor(Qt::blue));CHECK(onlinePhoto.pixelColor(0,0).alpha()==0);
  CHECK(Bee::avatarForUser(peer,QSize(40,40),false).toImage()==Bee::avatarForUser(offlinePeer,QSize(40,40),false).toImage());
  peer.setNetworkAddress(NetworkAddress(QHostAddress("127.0.0.2"),45001));UserManager::instance().setUser(peer);settings.setSaveUserList(true);settings.setRemoveInactiveUsers(false);CoreProbe core;
  Group group;group.setId(4001);group.setName("avatar-fixture");group.setChatType(Group::PrivateChat);group.addUser(ID_LOCAL_USER);group.addUser(peer.id());Chat chat;chat.setGroup(group);
  for(int i=0;i<20;++i){Message message(Message::Chat,5000+i,QString("avatar-message-%1").arg(i));chat.addMessage(ChatMessage(i%2?ID_LOCAL_USER:peer.id(),message,ChatMessage::Chat,true));}ChatManager::instance().setChat(chat);
  GuiUserList contacts;contacts.setUser(local,false);contacts.setUser(peer,false);QTreeWidget* tree=contacts.findChild<QTreeWidget*>();CHECK(tree);CHECK(peerRow(tree,peer.id()));CHECK(!peerRow(tree,ID_LOCAL_USER));
  GuiFloatingChat window;window.setEmbeddedMode(true);window.resize(850,650);window.guiChat()->setModernMode(true);CHECK(window.setChat(chat));window.show();QCoreApplication::processEvents();
  QTextBrowser* viewer=window.findChild<QTextBrowser*>("GuiChatViewer");CHECK(viewer);QTextDocument* document=viewer->document();const QString text=document->toPlainText(),oldLocalUrl=avatarUrl(document,ID_LOCAL_USER),oldPeerUrl=avatarUrl(document,peer.id());CHECK(!oldLocalUrl.isEmpty() && !oldPeerUrl.isEmpty() && oldLocalUrl!=oldPeerUrl);CHECK(centerIs(document,oldLocalUrl,Qt::red));CHECK(centerIs(document,oldPeerUrl,Qt::blue));
  int changed=0;QObject::connect(&core,&Core::userChanged,[&](const User& user){++changed;contacts.setUser(user,false);window.updateUser(user);});
  VCard newOwn=own;newOwn.setPhoto(photo(Qt::green));CHECK(core.setLocalUserVCard(local.color(),newOwn));QCoreApplication::processEvents();
  const QString newLocalUrl=avatarUrl(document,ID_LOCAL_USER);CHECK(newLocalUrl!=oldLocalUrl);CHECK(centerIs(document,newLocalUrl,Qt::green));CHECK(avatarUrl(document,peer.id())==oldPeerUrl);CHECK(centerIs(document,oldPeerUrl,Qt::blue));CHECK(UserManager::instance().findUser(peer.id()).vCard().photoHash()==other.photoHash());CHECK(document->toPlainText()==text);
  const QImage contactBefore=peerRow(tree,peer.id())->icon(0).pixmap(settings.avatarIconSize()).toImage();CHECK(QColor(contactBefore.pixel(contactBefore.width()/2,contactBefore.height()/2)).rgb()==QColor(Qt::blue).rgb());
  User nextPeer=peer;VCard newPeerCard=other;newPeerCard.setPhoto(photo(Qt::yellow));nextPeer.setVCard(newPeerCard);
  const User currentLocal=settings.localUser();settings.setLocalUser(nextPeer);const Message peerUpdate=Protocol::instance().localVCardMessage(peer.protocolVersion());settings.setLocalUser(currentLocal);core.parseUserMessage(peer,peerUpdate);QCoreApplication::processEvents();
  const QString newPeerUrl=avatarUrl(document,peer.id());CHECK(newPeerUrl!=oldPeerUrl);CHECK(centerIs(document,newPeerUrl,Qt::yellow));CHECK(avatarUrl(document,ID_LOCAL_USER)==newLocalUrl);CHECK(centerIs(document,newLocalUrl,Qt::green));CHECK(document->toPlainText()==text);CHECK(changed==2);
  const QImage contactAfter=peerRow(tree,peer.id())->icon(0).pixmap(settings.avatarIconSize()).toImage();CHECK(QColor(contactAfter.pixel(contactAfter.width()/2,contactAfter.height()/2)).rgb()==QColor(Qt::yellow).rgb());CHECK(settings.localUser().vCard().photoHash()==newOwn.photoHash());
  Message following(Message::Chat,99999,"after-avatar-update");const ChatMessage message(peer.id(),following,ChatMessage::Chat,true);chat.addMessage(message);ChatManager::instance().setChat(chat);window.showChatMessage(chat,message);CHECK(avatarUrl(document,peer.id())==newPeerUrl);CHECK(document->toPlainText().contains("after-avatar-update"));
  core.saveUsersAndGroups();settings.save();CHECK(childReload(temporary.path(),false));
  const QString afterAppend=document->toPlainText();User clearedPeer=UserManager::instance().findUser(peer.id());VCard cleared=clearedPeer.vCard();cleared.setPhoto(QPixmap());clearedPeer.setVCard(cleared);
  const User ownAfter=settings.localUser();settings.setLocalUser(clearedPeer);const Message clearedUpdate=Protocol::instance().localVCardMessage(peer.protocolVersion());settings.setLocalUser(ownAfter);core.parseUserMessage(UserManager::instance().findUser(peer.id()),clearedUpdate);QCoreApplication::processEvents();
  CHECK(avatarUrl(document,peer.id())!=newPeerUrl);CHECK(UserManager::instance().findUser(peer.id()).vCard().photo().isNull());CHECK(avatarUrl(document,ID_LOCAL_USER)==newLocalUrl);CHECK(document->toPlainText()==afterAppend);CHECK(changed==3);
  core.saveUsersAndGroups();settings.save();CHECK(childReload(temporary.path(),true));
  QDir().mkpath("E:/ShadowChat/delivery");window.grab().save("E:/ShadowChat/delivery/avatar-refresh-proof.png");
  std::puts("PASS distinct local/contact identity and photos; actual Core local update and parsed peer VCard refresh existing transcript/cache and contact icon; text preserved");return 0;
}
