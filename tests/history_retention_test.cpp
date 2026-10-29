#include "BeeApplication.h"
#include "Settings.h"
#include "MessageManager.h"
#include "ChatManager.h"
#include "BuildSavedChatList.h"
#include "UserManager.h"
#include "Core.h"
#include "Message.h"
#include "Protocol.h"
#include "IconManager.h"
#include "Random.h"
#include "SaveChatList.h"
#include <QTemporaryDir>
#include <QProcess>
#include <cstdio>

#define CHECK(value) do{if(!(value)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#value);return 1;}}while(false)
static const int recordCount=10051;
static QString marker(const QString& prefix,int i){return prefix+QString("%1").arg(i,5,10,QLatin1Char('0'));}
static Chat liveChat()
{
  Group group;group.setId(4500);group.setName("retention-live");group.setChatType(Group::PrivateChat);group.addUser(ID_LOCAL_USER);group.addUser(4501);
  Chat chat;chat.setGroup(group);return chat;
}
static bool intact(const QString& html,const QString& prefix)
{
  return html.contains(marker(prefix,0)) && html.contains(marker(prefix,recordCount-1)) && html.count(QRegularExpression(prefix+"[0-9]{5}"))>=recordCount;
}
static int worker(int argc,char** argv)
{
  BeeApplication app(argc,argv);
  const QString mode=app.arguments().at(1),root=app.arguments().at(2);
  Settings& settings=Settings::instance();settings.setDefaultFolders();settings.createLocalUser("retention-fixture");settings.setAccountDataFolder(root);
  if(mode=="seed") {QSettings legacy(QDir(root).filePath("beebeep.ini"),QSettings::IniFormat);legacy.setValue("Gui/ChatMaxLineSaved",9000);legacy.sync();}
  settings.load();CHECK(settings.chatMaxLineSaved()==-1);
  Random::init();MessageManager::instance().loadSavedMessagesAuthCode();Protocol::instance();IconManager::instance();UserManager::instance();ChatManager::instance();
  if(mode=="seed") {
    Chat chat=liveChat();
    QString archived;
    for(int i=0;i<recordCount;++i){
      Message message(Message::Chat,100000+i,marker("retention-live-",i));
      chat.addMessage(ChatMessage(ID_LOCAL_USER,message,ChatMessage::Chat,true));
      archived+=marker("retention-archive-",i)+"<br>";
    }
    ChatManager::instance().setChat(chat);
    QMap<QString,QString> old;old.insert("retention-archive",archived);ChatManager::instance().addSavedChats(old);
    QFile attachment(QDir(root).filePath("keep-asset.psd"));CHECK(attachment.open(QIODevice::WriteOnly));CHECK(attachment.write("keep-local-file")==15);attachment.close();
    CHECK(MessageManager::instance().saveMessages(true));settings.save();
    std::printf("PASS production save: live %d plus archived %d records\n",recordCount,recordCount);
    return 0;
  }
  BuildSavedChatList loader;loader.buildList();
  const QMap<QString,QString> saved=loader.savedChats();
  CHECK(intact(saved.value("retention-archive"),"retention-archive-"));
  if(mode=="cleared") {
    CHECK(!saved.value("retention-live").contains("retention-live-"));
    QFile attachment(QDir(root).filePath("keep-asset.psd"));CHECK(attachment.open(QIODevice::ReadOnly));CHECK(attachment.readAll()=="keep-local-file");
    std::puts("PASS fresh restart after manual clear: selected history gone, archived history and local attachment retained");return 0;
  }
  CHECK(intact(saved.value("retention-live"),"retention-live-"));
  if(mode=="verify") {
    QFile original(settings.savedChatsFilePath());CHECK(original.open(QIODevice::ReadOnly));
    const QByteArray prior=original.readAll();original.close();CHECK(!prior.isEmpty());
    settings.setChatAutoSave(false);SaveChatList saver;CHECK(!saver.save());
    CHECK(original.open(QIODevice::ReadOnly));CHECK(original.readAll()==prior);original.close();
    settings.setChatAutoSave(true);
    std::puts("PASS disabling future history saving preserves existing database bytes");
  }
  ChatManager::instance().addSavedChats(saved);
  if(mode=="clear") {
    Core core;ChatManager::instance().setChat(liveChat());
    CHECK(core.clearMessagesInChat(4500,true));
    CHECK(MessageManager::instance().saveMessages(true));settings.save();
    std::puts("PASS explicit Core.clearMessagesInChat and production save");return 0;
  }
  CHECK(MessageManager::instance().saveMessages(true));settings.save();
  std::puts("PASS fresh restart: first and last records preserved beyond old 9000 cap; re-save preserved recovered history");return 0;
}
int main(int argc,char** argv)
{
  if(argc>1)return worker(argc,argv);
  QCoreApplication app(argc,argv);QTemporaryDir directory;CHECK(directory.isValid());
  QProcessEnvironment environment=QProcessEnvironment::systemEnvironment();environment.insert("QT_QPA_PLATFORM","offscreen");
  foreach(const QString& mode,QStringList()<<"seed"<<"verify"<<"clear"<<"cleared") {
    QProcess process;process.setProcessEnvironment(environment);process.setProcessChannelMode(QProcess::ForwardedChannels);process.start(app.applicationFilePath(),QStringList()<<mode<<directory.path());
    CHECK(process.waitForFinished(60000));CHECK(process.exitCode()==0);
  }
  std::puts("PASS 10051-record production history retention, restart, re-save and explicit clear");return 0;
}
