#include "Config.h"
#include "BeeApplication.h"
#include "BeeUtils.h"
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
#include <cstdio>

class PeerCore : public Core
{
public:
  using Core::newPeerFound;
};

static int runPeer( int argc, char** argv )
{
  BeeApplication app( argc, argv );
  qInstallMessageHandler( []( QtMsgType, const QMessageLogContext&, const QString& text ) {
    std::fprintf( stderr, "%s\n", qPrintable( text ) );
  } );
  const QString folder = app.arguments().at( 2 );
  const int role = app.arguments().at( 3 ).toInt();
  const int round = app.arguments().at( 4 ).toInt();
  Random::init();
  Settings& settings = Settings::instance();
  settings.setDefaultFolders();
  settings.createLocalUser( QString( "peer-%1" ).arg( role ) );
  settings.setAccountDataFolder( folder );
  settings.load();
  settings.m_defaultListenerPort = 47650 + role;
  settings.m_defaultBroadcastPort = 47750 + role;
  settings.m_defaultFileTransferPort = 47850 + role;
  settings.m_broadcastOnlyToHostsIni = true;
  settings.m_broadcastAddressesInFileHosts.clear();
  settings.m_broadcastToLocalSubnetAlways = false;
  settings.m_disableMulticast = true;
  settings.m_useMulticastDns = false;
  settings.m_allowMultipleInstances = true;
  settings.setPreventMultipleConnectionsFromSingleHostAddress( false );
  settings.m_useHive = false;
  settings.m_userRecognitionMethod = Settings::RecognizeByAccount;
  User local = settings.localUser();
  local.setName( QString( "peer-%1" ).arg( role ) );
  local.setAccountName( QString( "peer-%1@example.test" ).arg( role ) );
  local.setHash( settings.simpleHash( local.accountName() ) );
  local.setWorkgroups( QStringList{ QString( "initial-workgroup-%1" ).arg( role ) } );
  settings.setAcceptConnectionsOnlyFromWorkgroups( false );
  settings.setLocalUser( local );
  NetworkManager::instance().searchLocalHostAddress();
  Protocol::instance();
  UserManager::instance();
  MessageManager::instance().loadSavedMessagesAuthCode();
  ChatManager::instance();
  HistoryManager::instance();
  IconManager::instance();
  ColorManager::instance();
  app.init();
  PeerCore core;
  core.init();
  if( !core.start() ) return 2;
  QSet<QString> received;
  QSet<VNumber> sent;
  bool groupSent = false;
  bool noticeSeen = false;
  bool workgroupsSent = false;
  QElapsedTimer elapsed;
  elapsed.start();
  QObject::connect( &core, &Core::newChatMessage, &app, [&]( const Chat&, const ChatMessage& message ) {
    if( message.type() == ChatMessage::Chat && !message.isFromLocalUser() )
    {
      const QString text = Bee::removeHtmlTags( message.message() ).trimmed();
      if( text.startsWith( "direct-from-" ) || text.startsWith( "group-from-" ) ) received.insert( text );
    }
    if( message.isFromSystem() && message.message().contains( QStringLiteral( "已送达" ) ) ) noticeSeen = true;
  } );
  QTimer timer;
  QObject::connect( &timer, &QTimer::timeout, &app, [&]() {
    for( int remote = 0; remote < 3; ++remote )
      if( remote != role && (role == 0 || remote == 0 || elapsed.elapsed() >= 1500) )
        core.newPeerFound( QHostAddress::LocalHost, 47650 + remote );
    if( core.connectedUsers() != 2 ) return;
    if( !workgroupsSent )
    {
      core.setLocalUserWorkgroups( QStringList{ QString( "updated-workgroup-%1" ).arg( role ) } );
      workgroupsSent = true;
    }
    bool workgroupsReceived = true;
    for( const User& remoteUser : UserManager::instance().userList().toList() )
      if( !remoteUser.isLocal() && remoteUser.isStatusConnected() )
        workgroupsReceived = workgroupsReceived && remoteUser.workgroups().size() == 1 &&
          remoteUser.workgroups().first().startsWith( "updated-workgroup-" );
    for( const User& user : UserManager::instance().userList().toList() )
      if( !user.isLocal() && user.isStatusConnected() && !sent.contains( user.id() ) )
      {
        if( !Protocol::instance().isShadowChatUser( user ) ) { app.exit( 3 ); return; }
        core.createPrivateChat( user );
        for( const Chat& chat : ChatManager::instance().constChatList() )
          if( chat.isPrivate() && chat.privateUserId() == user.id() &&
              core.sendChatMessage( chat.id(), QString( "direct-from-%1" ).arg( role ), false, false, false ) == 1 )
            sent.insert( user.id() );
      }
    Chat groupChat;
    for( const Chat& chat : ChatManager::instance().constChatList() )
      if( chat.isGroup() ) groupChat = chat;
    if( role == 0 && !groupChat.isValid() )
    {
      Group group;
      group.setName( "three-peer-fixture" );
      for( const User& user : UserManager::instance().userList().toList() )
        if( user.isLocal() || user.isStatusConnected() ) group.addUser( user.id() );
      groupChat = core.createGroupChat( local, group, true );
    }
    if( groupChat.isValid() && groupChat.usersId().size() == 3 && !groupSent )
      groupSent = core.sendChatMessage( groupChat.id(), QString( "group-from-%1" ).arg( role ), false, false, false ) == 2;
    if( sent.size() == 2 && groupSent && received.size() == 4 && workgroupsReceived )
    {
      const QString readyRoot = QFileInfo( folder ).absolutePath();
      QFile ready( QDir( readyRoot ).filePath( QString( "round-%1-ready-%2" ).arg( round ).arg( role ) ) );
      if( !ready.open( QIODevice::WriteOnly ) || ready.write( "ready" ) != 5 )
      { app.exit( 6 ); return; }
      ready.close();
      bool everyoneReady = true;
      for( int remote = 0; remote < 3; ++remote )
        if( !QFileInfo::exists( QDir( readyRoot ).filePath( QString( "round-%1-ready-%2" ).arg( round ).arg( remote ) ) ) )
          everyoneReady = false;
      if( everyoneReady )
      {
        timer.stop();
        QTimer::singleShot( 1000, &app, [&]() { app.exit( noticeSeen ? 4 : 0 ); } );
      }
    }
  } );
  timer.start( 300 );
  QTimer::singleShot( 30000, &app, [&]() {
    std::fprintf( stderr, "TIMEOUT role=%d peers=%d sent=%d received=%d groupSent=%d\n",
      role,core.connectedUsers(),sent.size(),received.size(),groupSent );
    app.exit( 5 );
  } );
  const int result = app.exec();
  core.stop();
  app.cleanUp();
  return result;
}

int main( int argc, char** argv )
{
  if( argc > 1 && QByteArray( argv[1] ) == "peer" ) return runPeer( argc, argv );
  QCoreApplication app( argc, argv );
  QTemporaryDir directory;
  for( int round = 0; round < 2; ++round )
  {
    QProcess processes[3];
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert( "QT_QPA_PLATFORM", "offscreen" );
    for( int position = 0; position < 3; ++position )
    {
      const int role = round ? 2-position : position;
      processes[role].setProcessEnvironment( environment );
      processes[role].start( app.applicationFilePath(), QStringList() << "peer" <<
        directory.filePath( QString( "round-%1-peer-%2" ).arg( round ).arg( role ) ) <<
        QString::number( role ) << QString::number( round ) );
      QThread::msleep( 250 );
    }
    bool passed = true;
    for( int role = 0; role < 3; ++role )
    {
      if( !processes[role].waitForFinished( 35000 ) ) { processes[role].kill(); processes[role].waitForFinished(); }
      if( processes[role].exitStatus() != QProcess::NormalExit || processes[role].exitCode() )
      {
        std::fprintf( stderr, "role %d exit %d\n%s", role,processes[role].exitCode(),
          processes[role].readAllStandardError().constData() );
        passed = false;
      }
    }
    if( !passed ) return 1;
    std::printf( "PASS isolated three-process round%d six private directions and six group deliveries, no receipt rows\n", round );
  }
  std::puts( "Loopback with explicit peer discovery only; NOT real-office-network acceptance." );
  return 0;
}
