#include "BeeApplication.h"
#include "Settings.h"
#include "GuiChatList.h"
#include "ShadowConversationPreferences.h"
#include "MessageManager.h"
#include "BuildSavedChatList.h"
#include "ChatManager.h"
#include "UserManager.h"
#include "Message.h"
#include "Random.h"
#include <QTemporaryDir>
#include <QProcess>
#include <cstdio>
#define CHECK(value) do{if(!(value)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#value);return 1;}}while(false)

class ListProbe : public GuiChatList {public:ListProbe():GuiChatList(nullptr){} using GuiChatList::showChatMenu;};
static Chat createChat(VNumber id,const QString& name,VNumber peer,bool group=false)
{
  Group members;members.setId(id);members.setName(name);members.setChatType(group?Group::GroupChat:Group::PrivateChat);members.addUser(ID_LOCAL_USER);members.addUser(peer);if(group)members.setPrivateId("stable-project-group");
  Chat chat;chat.setGroup(members);Message message(Message::Chat,id+100,"keep-message-"+name);chat.addMessage(ChatMessage(ID_LOCAL_USER,message,ChatMessage::Chat,true));return chat;
}
static GuiChatItem* row(QTreeWidget* tree,VNumber id)
{
  for(int i=0;i<tree->topLevelItemCount();++i){GuiChatItem* item=dynamic_cast<GuiChatItem*>(tree->topLevelItem(i));if(item && item->chatId()==id)return item;}return nullptr;
}
static bool menu(ListProbe& list,QTreeWidget* tree,VNumber id,const QString& actionName,bool* checked=nullptr)
{
  GuiChatItem* item=row(tree,id);if(!item || item->isHidden())return false;
  bool triggered=false;
  QTimer::singleShot(0,[&](){QMenu* popup=qobject_cast<QMenu*>(QApplication::activePopupWidget());if(!popup)return;foreach(QAction* action,popup->actions())if(action->text()==actionName){if(checked)*checked=action->isChecked();triggered=true;action->trigger();break;}popup->close();});
  list.showChatMenu(tree->visualItemRect(item).center());QCoreApplication::processEvents();return triggered;
}
static int worker(int argc,char** argv)
{
  BeeApplication app(argc,argv);QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
  const QString root=app.arguments().at(2);const bool restart=app.arguments().at(1)=="restart";
  Settings& settings=Settings::instance();settings.setDefaultFolders();settings.createLocalUser("preferences-fixture");settings.setAccountDataFolder(root);settings.load();settings.setHideEmptyChatsInList(false);Random::init();MessageManager::instance().loadSavedMessagesAuthCode();
  const VNumber base=restart?2000:1000;
  User alpha(base+1),beta(base+2);alpha.setName("alpha");alpha.setAccountName("alpha@example.test");alpha.setHash("stable-alpha-hash");alpha.setStatus(User::Online);beta.setName("beta");beta.setAccountName("beta@example.test");beta.setHash("stable-beta-hash");beta.setStatus(User::Online);UserManager::instance().setUser(alpha);UserManager::instance().setUser(beta);
  Chat a=createChat(base+11,"alpha",alpha.id()),b=createChat(base+12,"beta",beta.id()),group=createChat(base+13,"project",alpha.id(),true);
  a.setLastMessageTimestamp(QDateTime::currentDateTime().addDays(-1));b.setLastMessageTimestamp(QDateTime::currentDateTime());b.addUnreadMessage();
  ChatManager::instance().setChat(a);ChatManager::instance().setChat(b);ChatManager::instance().setChat(group);
  ListProbe list;list.resize(400,500);list.show();list.updateChats();QCoreApplication::processEvents();QTreeWidget* tree=list.findChild<QTreeWidget*>();CHECK(tree);
  int clearRequests=0;QObject::connect(&list,&GuiChatList::chatToClear,[&](VNumber){++clearRequests;});list.selectFirstChat();CHECK(clearRequests==0);
  ShadowConversationPreferences preferences;
  const QString historyPath=settings.savedChatsFilePath();
  if(!restart) {
    CHECK(MessageManager::instance().saveMessages(true));settings.save();QFile history(historyPath);CHECK(history.open(QIODevice::ReadOnly));const QByteArray original=history.readAll();history.close();
    QFile attachment(QDir(root).filePath("keep.psd"));CHECK(attachment.open(QIODevice::WriteOnly));attachment.write("keep-original");attachment.close();
    CHECK(menu(list,tree,a.id(),QString::fromUtf8("置顶会话")));CHECK(preferences.pinned(a));CHECK(row(tree,a.id())->data(0,GuiChatItem::ChatPinned).toBool());CHECK(dynamic_cast<GuiChatItem*>(tree->topLevelItem(0))->chatId()==a.id());
    CHECK(menu(list,tree,a.id(),QString::fromUtf8("取消置顶")));CHECK(!preferences.pinned(a));CHECK(menu(list,tree,a.id(),QString::fromUtf8("置顶会话")));CHECK(preferences.pinned(a));
    CHECK(menu(list,tree,a.id(),QString::fromUtf8("消息免打扰")));CHECK(preferences.muted(a));CHECK(row(tree,a.id())->data(0,GuiChatItem::ChatMuted).toBool());
    CHECK(menu(list,tree,a.id(),QString::fromUtf8("标为待处理")));CHECK(preferences.pending(a));CHECK(row(tree,a.id())->data(0,GuiChatItem::ChatPending).toBool());
    bool checked=false;CHECK(menu(list,tree,a.id(),QString::fromUtf8("消息免打扰"),&checked));CHECK(checked && !preferences.muted(a));CHECK(menu(list,tree,a.id(),QString::fromUtf8("消息免打扰")));CHECK(preferences.muted(a));
    CHECK(menu(list,tree,a.id(),QString::fromUtf8("移除会话")));CHECK(row(tree,a.id())->isHidden());CHECK(preferences.hidden(a));
    const QList<VNumber> members=group.usersId();CHECK(menu(list,tree,group.id(),QString::fromUtf8("移除会话")));CHECK(preferences.hidden(group));CHECK(ChatManager::instance().chat(group.id()).usersId()==members);CHECK(ChatManager::instance().chat(a.id()).messages().size()==a.messages().size());
    CHECK(history.open(QIODevice::ReadOnly));CHECK(history.readAll()==original);history.close();
    std::puts("PASS real QWidget context actions: pinned first, muted, private/group hidden; membership and history bytes unchanged");return 0;
  }
  CHECK(preferences.pinned(a) && preferences.muted(a) && preferences.pending(a) && preferences.hidden(a) && preferences.hidden(group));CHECK(row(tree,a.id())->isHidden());CHECK(row(tree,group.id())->isHidden());CHECK(!row(tree,b.id())->isHidden());
  BuildSavedChatList loader;loader.buildList();const QString saved=loader.savedChats().values().join("\n");CHECK(saved.contains("keep-message-alpha") && saved.contains("keep-message-project"));
  CHECK(preferences.reveal(a));list.updateChat(a);CHECK(!row(tree,a.id())->isHidden());
  CHECK(preferences.setHidden(a,true));list.updateChat(a);CHECK(row(tree,a.id())->isHidden());Message fresh(Message::Chat,99999,"fresh-message");a.addMessage(ChatMessage(alpha.id(),fresh,ChatMessage::Chat,true));ChatManager::instance().setChat(a);CHECK(preferences.reveal(a));list.updateChat(a);CHECK(!row(tree,a.id())->isHidden());
  settings.setAccountDataFolder(QDir(root).filePath("another-account"));list.updateChats();CHECK(!ShadowConversationPreferences().pinned(a));CHECK(!ShadowConversationPreferences().muted(a));CHECK(!row(tree,group.id())->isHidden());
  settings.setAccountDataFolder(root);list.updateChats();CHECK(preferences.pinned(a) && preferences.muted(a));CHECK(row(tree,group.id())->isHidden());
  QFile attachment(QDir(root).filePath("keep.psd"));CHECK(attachment.open(QIODevice::ReadOnly));CHECK(attachment.readAll()=="keep-original");
  std::puts("PASS new process with changed numeric IDs: stable peer/group preferences, reveal, account isolation and history/file preservation");return 0;
}
int main(int argc,char** argv)
{
  if(argc>1)return worker(argc,argv);QCoreApplication app(argc,argv);QTemporaryDir directory;CHECK(directory.isValid());QProcessEnvironment environment=QProcessEnvironment::systemEnvironment();environment.insert("QT_QPA_PLATFORM","offscreen");
  foreach(const QString& mode,QStringList()<<"seed"<<"restart"){QProcess process;process.setProcessEnvironment(environment);process.setProcessChannelMode(QProcess::ForwardedChannels);process.start(app.applicationFilePath(),QStringList()<<mode<<directory.path());CHECK(process.waitForFinished(30000));CHECK(process.exitCode()==0);}
  std::puts("PASS conversation preferences actual QWidget and restart integration");return 0;
}
