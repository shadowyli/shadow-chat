#include "Config.h"
#include "BeeApplication.h"
// Test-only configuration access: no account bypass or test hook ships in the app.
#define private public
#include "Settings.h"
#undef private
#include "Core.h"
#include "NetworkManager.h"
#include "Protocol.h"
#include "UserManager.h"
#include "ChatManager.h"
#include "MessageManager.h"
#include "HistoryManager.h"
#include "IconManager.h"
#include "ColorManager.h"
#include "Random.h"
#include "BuildSavedChatList.h"
#include "ShadowWorkspaceArchive.h"
#include "HistoryRecord.h"
#include <QTemporaryDir>
#include <QProcess>
#include <cstdio>
#ifdef Q_OS_WIN
#include <windows.h>
#include <winioctl.h>
#include <io.h>
#endif

class LoopbackCore : public Core { public: using Core::newPeerFound; };

static int restoredHistory(int argc,char** argv)
{
  BeeApplication app(argc,argv);
  Settings& settings=Settings::instance(); settings.setDefaultFolders(); settings.createLocalUser("history-check"); settings.setAccountDataFolder(app.arguments().at(2)); settings.load();
  MessageManager::instance().loadSavedMessagesAuthCode();
  BuildSavedChatList loader; loader.buildList();
  const QString html=loader.savedChats().values().join("\n");
  bool found=html.contains("loopback-message-from-alpha") && html.contains("loopback-message-from-beta");
  if(app.arguments().value(3)=="migrated") {
    QSettings mapping(QDir(settings.dataFolder()).filePath(".shadow-paths.ini"),QSettings::IniFormat);
    const QString target=mapping.value("targetRoot").toString();
    const QString oldRoot=mapping.value("sourceRoot").toString();
    QFile file(QDir(target).filePath("beta-asset.txt"));
    const QString targetUrl=QUrl::fromLocalFile(target).toString();
    const QString encodedTargetUrl=QUrl::fromLocalFile(target).toString(QUrl::FullyEncoded);
    found=found && !target.isEmpty() && file.open(QIODevice::ReadOnly) && file.readAll()=="asset-from-beta" &&
      (html.contains(targetUrl) || html.contains(encodedTargetUrl));
    bool parsedFile=false;
    foreach(const HistoryRecord& record,shadowChatHistoryRecords(html))foreach(const QString& address,record.files) {
      const QString local=QDir::cleanPath(QUrl(address).toLocalFile());
      if(local==QDir::cleanPath(QDir(target).filePath("beta-asset.txt")))parsedFile=true;
      if(!oldRoot.isEmpty() && local.startsWith(QDir::cleanPath(oldRoot)+"/"))found=false;
    }
    found=found && parsedFile;
    std::printf("Actual chat backup restore with relocated attachment: %s\n",found?"PASS":"FAIL");
  }
  std::printf("Restart history verification: %s\n",found?"PASS":"FAIL");
  return found?0:4;
}

static int peer(int argc,char** argv)
{
  BeeApplication app(argc,argv);
  qInstallMessageHandler([](QtMsgType,const QMessageLogContext&,const QString& text){std::fprintf(stderr,"%s\n",qPrintable(text));});
  const QString root=app.arguments().at(2);
  const QString name=app.arguments().at(3);
  const int port=app.arguments().at(4).toInt();
  const int remotePort=app.arguments().at(5).toInt();
  const qint64 largeSize=app.arguments().value(6).toLongLong();
  const bool capacity=app.arguments().value(7)=="capacity";
  Random::init();
  Settings& settings=Settings::instance();
  settings.setDefaultFolders(); settings.createLocalUser(name); settings.setAccountDataFolder(root); settings.load();
  settings.m_defaultListenerPort=port;
  settings.m_defaultBroadcastPort=port+100;
  settings.m_defaultFileTransferPort=port+200;
  settings.m_broadcastOnlyToHostsIni=true;
  settings.m_broadcastAddressesInFileHosts.clear();
  settings.m_broadcastToLocalSubnetAlways=false;
  settings.m_disableMulticast=true;
  settings.m_useMulticastDns=false;
  settings.m_allowMultipleInstances=true;
  settings.m_useHive=false;
  settings.setAskChangeUserAtStartup(false);
  settings.setDownloadDirectory(QDir(root).filePath("received")); QDir().mkpath(settings.downloadDirectory());
  User local=settings.localUser(); local.setName(name); local.setAccountName(name+"@example.test"); local.setHash(settings.simpleHash(name)); settings.setLocalUser(local);
  NetworkManager::instance().searchLocalHostAddress();
  settings.setLocalUserHost(QHostAddress::LocalHost,port);
  Protocol::instance(); UserManager::instance(); MessageManager::instance().loadSavedMessagesAuthCode(); ChatManager::instance(); HistoryManager::instance(); IconManager::instance(); ColorManager::instance();
  app.init();
  LoopbackCore core; core.init();
  QObject::connect(&app,&BeeApplication::tickEvent,&core,&Core::onTickEvent);
  bool textReceived=false, sent=false, fileReceived=false, fileUploaded=false;
  QSet<QString> sentTo,receivedFrom;
  QObject::connect(&core,&Core::newChatMessage,[&](const Chat&,const ChatMessage& message){ if(message.message().contains("loopback-message-from-") && !message.message().contains("from-"+name)) {textReceived=true; receivedFrom.insert(message.message()); std::fprintf(stderr,"TEXT RECEIVED %s\n",qPrintable(name));} });
  QObject::connect(&core,&Core::fileDownloadRequest,[&](const User& user,const FileInfo& info){
    FileInfo target=info;
    target.setPath(QDir(settings.downloadDirectory()).filePath(info.name()));
    std::fprintf(stderr,"DOWNLOAD REQUEST %s\n",qPrintable(info.name())); core.downloadFile(user.id(),target,false);
  });
  QObject::connect(&core,&Core::fileTransferMessage,[&](VNumber,const User&,const FileInfo& info,const QString&,FileTransferPeer::TransferState state){ if(state==FileTransferPeer::Completed) { if(info.isDownload()) fileReceived=true; else fileUploaded=true; } });
  if(!core.start()) return 2;
  QTimer timer;
  QObject::connect(&timer,&QTimer::timeout,[&](){
    if(capacity) {
      if(name=="node0") for(int i=1;i<20;++i)core.newPeerFound(QHostAddress::LocalHost,47440+i);
      foreach(const User& user,UserManager::instance().userList().toList()) {
        if(user.isLocal() || !user.isStatusConnected() || sentTo.contains(user.hash()))continue;
        core.createPrivateChat(user);
        const Chat chat=ChatManager::instance().privateChatForUser(user.id());
        if(!chat.isValid())continue;
        core.sendChatMessage(chat.id(),"loopback-message-from-"+name,false,false,false);
        sentTo.insert(user.hash());
      }
      const int expected=name=="node0"?19:1;
      if(sentTo.size()==expected && receivedFrom.size()==expected && MessageManager::instance().countUnsentMessages(ID_INVALID,ID_INVALID)==0) {
        std::printf("PASS capacity %s sent=%d received=%d all ACKed\n",qPrintable(name),sentTo.size(),receivedFrom.size());
        timer.stop(); QTimer::singleShot(1000,&app,[&](){app.exit(0);});
      }
      return;
    }
    if(name=="alpha" && core.connectedUsers()==0) core.newPeerFound(QHostAddress::LocalHost,remotePort);
    foreach(const User& user,UserManager::instance().userList().toList()) {
      if(!user.isLocal() && user.isStatusConnected() && !sent) {
        core.createPrivateChat(user);
        const Chat chat=ChatManager::instance().privateChatForUser(user.id());
        if(!chat.isValid())continue;
        core.sendChatMessage(chat.id(),"loopback-message-from-"+name,false,false,false);
        const QString source=QDir(root).filePath(name+"-asset.txt"); QFile f(source);
        if(f.open(QIODevice::WriteOnly)){
#ifdef Q_OS_WIN
          if(largeSize>0) {DWORD returned=0; DeviceIoControl(reinterpret_cast<HANDLE>(_get_osfhandle(f.handle())),FSCTL_SET_SPARSE,NULL,0,NULL,0,&returned,NULL);}
#endif
          f.write(("asset-from-"+name).toUtf8());
          if(largeSize>0){ f.resize(largeSize); f.seek(largeSize-16); f.write("large-asset-tail!",16); }
          f.close();
        }
        core.sendFilesFromChat(chat.id(),QStringList()<<source);
        sent=true;
      }
    }
    core.processPendingFileTransfers();
    if(textReceived && fileReceived && fileUploaded) { std::puts("PASS peer text and file received and upload acknowledged"); QTimer::singleShot(500,&app,[&](){app.exit(0);}); timer.stop(); }
  });
  timer.start(500);
  QTimer::singleShot(app.arguments().value(7)=="large"?240000:30000,&app,[&](){std::fprintf(stderr,"TIMEOUT %s text=%d file=%d sent=%d connected=%d\n",qPrintable(name),textReceived,fileReceived,sent,core.connectedUsers()); app.exit(3);});
  const int result=app.exec();
  core.stop(); settings.save(); app.cleanUp();
  return result;
}

int main(int argc,char** argv)
{
  if(argc>1 && QByteArray(argv[1])=="peer") return peer(argc,argv);
  if(argc>1 && QByteArray(argv[1])=="history") return restoredHistory(argc,argv);
  QCoreApplication app(argc,argv);
  QTemporaryDir directory;
  if(!directory.isValid()) return 1;
  if(app.arguments().contains("--twenty")) {
    QList<QProcess*> peers;
    QProcessEnvironment environment=QProcessEnvironment::systemEnvironment(); environment.insert("QT_QPA_PLATFORM","offscreen");
    for(int i=0;i<20;++i){
      QProcess* process=new QProcess(&app); process->setProcessEnvironment(environment); process->setProcessChannelMode(QProcess::ForwardedChannels);
      const QString name="node"+QString::number(i);
      process->start(app.applicationFilePath(),QStringList()<<"peer"<<directory.filePath(name)<<name<<QString::number(47440+i)<<"47440"<<"0"<<"capacity"); peers<<process;
    }
    bool passed=true;
    for(int i=0;i<peers.size();++i){QProcess* process=peers.at(i);if(process->state()!=QProcess::NotRunning && !process->waitForFinished(60000)){process->kill();process->waitForFinished();passed=false;}if(process->exitCode()!=0)passed=false;std::printf("Capacity peer%d exit%d\n",i,process->exitCode());}
    std::printf("20-process capacity verification: %s\n",passed?"PASS":"FAIL");
    return passed?0:1;
  }
  QProcess alpha,beta;
  alpha.setProcessChannelMode(QProcess::ForwardedChannels); beta.setProcessChannelMode(QProcess::ForwardedChannels);
  const bool large=app.arguments().contains("--large");
  QProcessEnvironment environment=QProcessEnvironment::systemEnvironment(); environment.insert("QT_QPA_PLATFORM","offscreen");
  alpha.setProcessEnvironment(environment); beta.setProcessEnvironment(environment);
  beta.start(app.applicationFilePath(),QStringList()<<"peer"<<directory.filePath("beta")<<"beta"<<"47441"<<"47440"<<"0"<<(large?"large":"small"));
  alpha.start(app.applicationFilePath(),QStringList()<<"peer"<<directory.filePath("alpha")<<"alpha"<<"47440"<<"47441"<<(large?QString::number(5LL*1024*1024*1024):"0")<<(large?"large":"small"));
  if(!alpha.waitForFinished(250000)){alpha.kill();alpha.waitForFinished();}
  if(!beta.waitForFinished(250000)){beta.kill();beta.waitForFinished();}
  std::printf("ALPHA exit %d\n%s\n%s\nBETA exit %d\n%s\n%s\n",alpha.exitCode(),alpha.readAllStandardOutput().constData(),alpha.readAllStandardError().constData(),beta.exitCode(),beta.readAllStandardOutput().constData(),beta.readAllStandardError().constData());
  QFile alphaReceived(directory.filePath("alpha/received/beta-asset.txt"));
  QFile betaReceived(directory.filePath("beta/received/alpha-asset.txt"));
  bool bytesMatch=alphaReceived.open(QIODevice::ReadOnly) && betaReceived.open(QIODevice::ReadOnly) && alphaReceived.readAll()=="asset-from-beta";
  if(large) {
    QFile source(directory.filePath("alpha/alpha-asset.txt"));
    QCryptographicHash expected(QCryptographicHash::Sha256),actual(QCryptographicHash::Sha256);
    bytesMatch=bytesMatch && source.open(QIODevice::ReadOnly) && betaReceived.size()==5LL*1024*1024*1024 && expected.addData(&source) && actual.addData(&betaReceived) && expected.result()==actual.result();
  } else bytesMatch=bytesMatch && betaReceived.readAll()=="asset-from-alpha";
  std::printf("Received file byte verification: %s\n",bytesMatch?"PASS":"FAIL");
  bool historyValid=true;
  foreach(const QString& name,QStringList()<<"alpha"<<"beta") {
    QProcess history; history.setProcessEnvironment(environment); history.start(app.applicationFilePath(),QStringList()<<"history"<<directory.filePath(name));
    if(!history.waitForFinished(30000)){history.kill();history.waitForFinished();historyValid=false;}
    std::printf("%s %s\n",qPrintable(name),history.readAllStandardOutput().constData());
    historyValid=historyValid && history.exitCode()==0;
  }
  bool migrationValid=true;
  if(!large && alpha.exitCode()==0 && beta.exitCode()==0 && bytesMatch && historyValid) {
    const QString backup=directory.filePath("actual-chat.beebeep-workspace"),migrated=directory.filePath("migrated-alpha");
    QString error,restorePoint;
    migrationValid=ShadowWorkspaceArchive::exportWithAttachments(directory.filePath("alpha"),directory.filePath("alpha/received"),backup,&error) &&
      ShadowWorkspaceArchive::stageImport(migrated,backup,&error) && ShadowWorkspaceArchive::applyPendingImport(migrated,&restorePoint,&error);
    if(migrationValid) {
      QProcess verifier;verifier.setProcessEnvironment(environment);verifier.setProcessChannelMode(QProcess::ForwardedChannels);
      verifier.start(app.applicationFilePath(),QStringList()<<"history"<<migrated<<"migrated");
      migrationValid=verifier.waitForFinished(30000) && verifier.exitCode()==0;
    }
    std::printf("Live chat -> backup -> new profile -> new-process restore: %s %s\n",migrationValid?"PASS":"FAIL",qPrintable(error));
  }
  return alpha.exitCode()==0 && beta.exitCode()==0 && bytesMatch && historyValid && migrationValid ? 0 : 1;
}
