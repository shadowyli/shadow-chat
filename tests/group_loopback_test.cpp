#include "Config.h"
#include "BeeApplication.h"
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
#include "GuiMain.h"
#include <cstdio>
class GroupTestCore : public Core { public: using Core::newPeerFound; };

static Chat project()
{
  foreach( const Chat& chat, ChatManager::instance().constChatList() ) if( chat.isGroup() ) return chat;
  return Chat();
}

static int peer( int argc, char** argv )
{
  BeeApplication app( argc, argv );
  qInstallMessageHandler( []( QtMsgType, const QMessageLogContext&, const QString& text ) { std::fprintf( stderr, "%s\n", qPrintable( text ) ); } );
  const QString root = app.arguments().at( 2 ), name = app.arguments().at( 3 ), phase = app.arguments().at( 4 );
  const int port = name == "alpha" ? 47550 : (name == "beta" ? 47551 : 47552);
  const int otherPort = phase == "invite" ? (name == "beta" ? 47552 : 47551) : (name == "alpha" ? 47551 : 47550);
  Random::init(); Settings& settings = Settings::instance();
  settings.setDefaultFolders(); settings.createLocalUser( name ); settings.setAccountDataFolder( root ); settings.load();
  settings.m_defaultListenerPort = port; settings.m_defaultBroadcastPort = port + 100; settings.m_defaultFileTransferPort = port + 200;
  settings.m_broadcastOnlyToHostsIni = true; settings.m_broadcastAddressesInFileHosts.clear();
  settings.m_broadcastToLocalSubnetAlways = false; settings.m_disableMulticast = true;
  settings.m_useMulticastDns = false; settings.m_allowMultipleInstances = true; settings.m_useHive = false;
  settings.m_userRecognitionMethod = Settings::RecognizeByAccount;
  settings.m_maxSimultaneousDownloads = 12;
  settings.setAskChangeUserAtStartup( false ); settings.setDownloadDirectory( QDir( root ).filePath( "received" ) );
  QDir().mkpath( settings.downloadDirectory() );
  User local = settings.localUser(); local.setName( name ); local.setAccountName( name + "@example.test" ); local.setHash( settings.simpleHash( name ) ); settings.setLocalUser( local );
  NetworkManager::instance().searchLocalHostAddress(); settings.setLocalUserHost( QHostAddress::LocalHost, port );
  Protocol::instance(); UserManager::instance(); MessageManager::instance().loadSavedMessagesAuthCode();
  ChatManager::instance(); HistoryManager::instance(); IconManager::instance(); ColorManager::instance();
  app.init(); GroupTestCore core; core.init();
  core.buildSavedChatList();
  QScopedPointer<GuiMain> guiReceiver;
  if( name == "beta" && phase == "resume" )
  {
    guiReceiver.reset( new GuiMain );
    if( qEnvironmentVariableIsSet( "SHADOW_TEST_FOLDER_PATH_ONLY" ) )
    {
      QObject::disconnect( &core, Q_NULLPTR, guiReceiver.data(), Q_NULLPTR );
      QObject::connect( &core, SIGNAL( folderDownloadRequest( const User&, const QString&, const QList<FileInfo>& ) ),
                        guiReceiver.data(), SLOT( downloadFolder( const User&, const QString&, const QList<FileInfo>& ) ) );
    }
  }
  QObject::connect( &app, &BeeApplication::tickEvent, &core, &Core::onTickEvent );
  bool sent = false, received = false, denied = false, textAck = false, fileAck = false, transferring = false, dissolved = false, forbidden = false, repeated = false;
  int receivedFiles = 0, uploadedFiles = 0, incomingImagePreviews = 0;
  QSet<QString> incomingAlbums;
  QElapsedTimer elapsed; elapsed.start();
  QObject::connect( &core, &Core::newChatMessage, [&]( const Chat&, const ChatMessage& message ) {
    if( name == "beta" && phase == "images" && message.isImagePreview() )
    {
      const auto album = QRegularExpression( QStringLiteral( "<!--shadow-album:([^>]+)-->" ) ).match( message.message() );
      if( album.hasMatch() ) { incomingAlbums.insert( album.captured( 1 ) ); ++incomingImagePreviews; }
    }
    if( !message.isFromLocalUser() && message.type() == ChatMessage::Chat && message.message().contains( phase == "seed" ? "group-seed" : "offline-group" ) ) received = true;
    if( !message.isFromLocalUser() && message.type() == ChatMessage::Chat && message.message().contains( "must-not-arrive" ) ) forbidden = true;
    if( !message.isFromLocalUser() && message.type() == ChatMessage::Chat && message.message().contains( "after-rejoin" ) ) received = true;
    if( !message.isFromLocalUser() && message.type() == ChatMessage::Chat && message.message().contains( "owner-offline-" ) ) received = true;
    if( message.message().contains( QStringLiteral( "已送达 1/1 人" ) ) )
    {
      if( message.type() == ChatMessage::Other && message.message().contains( "offline-group" ) ) textAck = true;
      if( message.type() == ChatMessage::Other && message.message().contains( "after-rejoin" ) ) textAck = true;
      if( message.type() == ChatMessage::FileTransfer && message.message().contains( "materials" ) ) fileAck = true;
    }
  } );
  QObject::connect( &core, &Core::fileDownloadRequest, [&]( const User& user, const FileInfo& offered ) {
    if( guiReceiver ) return;
    FileInfo file = offered; file.setPath( QDir( settings.downloadDirectory() ).filePath( file.name() ) );
    core.downloadFile( user.id(), file, false );
  } );
  QObject::connect( &core, &Core::folderDownloadRequest, [&]( const User& user, const QString&, const QList<FileInfo>& files ) {
    if( guiReceiver ) return; // Exercise the production GuiMain receive path.
    foreach( FileInfo file, files )
    {
      file.setPath( QDir( settings.downloadDirectory() ).filePath( file.shareFolder() + "/" + file.name() ) );
      core.downloadFile( user.id(), file, false );
    }
  } );
  QObject::connect( &core, &Core::fileTransferMessage, [&]( VNumber, const User&, const FileInfo& file, const QString&, FileTransferPeer::TransferState state ) {
    if( file.isDownload() && state == FileTransferPeer::Completed ) ++receivedFiles;
    if( !file.isDownload() && state == FileTransferPeer::Completed ) ++uploadedFiles;
  } );
  QObject::connect( &core, &Core::chatRemoved, [&]( const Chat& chat ) { if( chat.isGroup() ) dissolved = true; } );
  if( !core.start() ) return 2;
  QTimer timer;
  QObject::connect( &timer, &QTimer::timeout, [&]() {
    if( name == "alpha" && phase != "queue" && core.connectedUsers() == 0 ) core.newPeerFound( QHostAddress::LocalHost, otherPort );
    if( phase == "invite" && name == "beta" && core.connectedUsers() == 0 ) core.newPeerFound( QHostAddress::LocalHost, otherPort );
    const Chat chat = project();
    if( phase == "invite" )
    {
      if( name == "beta" && !sent && chat.isValid() && core.connectedUsers() > 0 )
      {
        Group invited = chat.group();
        foreach( const User& user, UserManager::instance().userList().toList() ) if( !user.isLocal() && user.isStatusConnected() ) invited.addUser( user.id() );
        if( core.isLocalGroupOwner( chat.id() ) || !core.changeGroupChat( local, invited ) ) { app.exit( 14 ); return; }
        sent = true;
        QTimer::singleShot( 1000, &app, [&]() {
          core.sendChatMessage( project().id(), "owner-offline-B", false, false, false );
          const QString source = QDir( root ).filePath( "owner-offline.psd" );
          QFile material( source ); material.open( QIODevice::WriteOnly ); material.write( "offline-owner-material" ); material.close();
          core.sendFilesFromChat( project().id(), QStringList() << source );
        } );
      }
      if( name == "gamma" && received && !sent && chat.isValid() )
      {
        if( chat.group().ownerHash() != settings.simpleHash( "alpha" ) || chat.usersId().size() != 3 || core.isLocalGroupOwner( chat.id() ) ) { app.exit( 15 ); return; }
        sent = true; core.sendChatMessage( chat.id(), "owner-offline-C", false, false, false );
      }
      if( name == "beta" ? (received && uploadedFiles == 1) : (sent && receivedFiles == 1) )
      { timer.stop(); QTimer::singleShot( 1500, &app, [&]() { app.exit( 0 ); } ); }
    }
    else if( phase == "seed" )
    {
      if( name == "alpha" && !sent && core.connectedUsers() > 0 )
      {
        Group group; group.setName( "Project" );
        foreach( const User& user, UserManager::instance().userList().toList() ) if( !user.isLocal() && user.isStatusConnected() ) group.addUser( user.id() );
        const Chat created = core.createGroupChat( local, group, true );
        core.sendChatMessage( created.id(), "group-seed", false, false, false ); sent = true;
      }
      if( name == "beta" && chat.isValid() && !denied )
      {
        Group forged = chat.group(); forged.setName( "forged-name" );
        denied = !core.changeGroupChat( local, forged ) && project().name() == "Project" && !core.isLocalGroupOwner( chat.id() );
      }
      if( elapsed.elapsed() > 6500 && (name == "alpha" ? sent : (received && denied)) ) app.exit( 0 );
    }
    else if( phase == "images" && chat.isValid() && core.connectedUsers() > 0 )
    {
      if( name == "alpha" && !sent )
      {
        QStringList images;
        for( int i = 0; i < 2; ++i )
        {
          QImage image( 40 + i, 32, QImage::Format_ARGB32 ); image.fill( i ? QColor( "#4294ad" ) : QColor( "#ed5985" ) );
          const QString path = QDir( root ).filePath( QString( "image-%1.png" ).arg( i ) );
          if( !image.save( path ) ) { app.exit( 12 ); return; }
          images << path;
        }
        sent = core.sendFilesFromChat( chat.id(), images ) == 2;
      }
      if( name == "alpha" ? uploadedFiles == 2 : (receivedFiles == 2 && incomingAlbums.size() == 1 && incomingImagePreviews == 2) )
      { timer.stop(); QTimer::singleShot( 1500, &app, [&]() { app.exit( 0 ); } ); }
    }
    else if( phase == "queue" && !sent && chat.isValid() )
    {
      if( !core.isLocalGroupOwner( chat.id() ) || core.connectedUsers() != 0 ) { app.exit( 7 ); return; }
      core.sendChatMessage( chat.id(), "offline-group", false, false, false );
      const QString folder = QDir( root ).filePath( "materials/sub" ); QDir().mkpath( folder );
      QDir().mkpath( QDir( root ).filePath( "materials/empty/subempty" ) );
      QFile file( QDir( folder ).filePath( "asset.psd" ) ); file.open( QIODevice::WriteOnly ); file.write( "group-material" ); file.close();
      QFile empty( QDir( folder ).filePath( "empty.txt" ) ); empty.open( QIODevice::WriteOnly ); empty.close();
      for( int i = 0; i < 501; ++i )
      {
        QFile extra( QDir( folder ).filePath( QString( "part-%1.bin" ).arg( i ) ) );
        extra.open( QIODevice::WriteOnly ); extra.write( QByteArray::number( i ) ); extra.close();
      }
      if( core.sendFilesFromChat( chat.id(), QStringList() << QDir( root ).filePath( "materials" ) ) != 1 ) { app.exit( 8 ); return; }
      sent = true; QTimer::singleShot( 1000, &app, [&]() { app.exit( 0 ); } );
    }
    else if( phase == "resume" )
    {
      if( name == "alpha" && textAck && fileAck && !repeated )
      {
        repeated = true; fileAck = false;
        core.sendFilesFromChat( chat.id(), QStringList() << QDir( root ).filePath( "materials" ) );
      }
      else if( name == "alpha" ? (repeated && textAck && fileAck) : (received && receivedFiles == 1006) )
      { timer.stop(); QTimer::singleShot( 1500, &app, [&]() { app.exit( 0 ); } ); }
    }
    else if( phase == "rejoin" && name == "alpha" && !sent && chat.isValid() && core.connectedUsers() > 0 )
    {
      User remote;
      foreach( const User& user, UserManager::instance().userList().toList() ) if( !user.isLocal() && user.isStatusConnected() ) remote = user;
      if( !remote.isValid() ) return;
      const QString source = QDir( root ).filePath( "must-not-arrive.txt" );
      QFile canceled( source ); canceled.open( QIODevice::WriteOnly ); canceled.write( "canceled" ); canceled.close();
      core.sendFilesFromChat( chat.id(), QStringList() << source );
      core.sendChatMessage( chat.id(), "must-not-arrive", false, true, false );
      Group removed = chat.group(); removed.removeUser( remote.id() );
      if( !core.changeGroupChat( local, removed ) ) { app.exit( 9 ); return; }
      sent = true;
      const VNumber remoteId = remote.id();
      QTimer::singleShot( 3000, &app, [&, remoteId]() {
        Group invited = project().group(); invited.addUser( remoteId );
        if( !core.changeGroupChat( local, invited ) ) { app.exit( 10 ); return; }
        QTimer::singleShot( 1000, &app, [&]() { core.sendChatMessage( project().id(), "after-rejoin", false, false, false ); } );
      } );
    }
    else if( phase == "roles" && chat.isValid() && core.connectedUsers() > 0 )
    {
      if( name == "alpha" && !transferring && elapsed.elapsed() > 2000 )
      {
        foreach( const User& user, UserManager::instance().userList().toList() )
          if( !user.isLocal() && user.isStatusConnected() ) transferring = core.transferGroupOwnership( chat.id(), user.id() );
      }
      if( name == "beta" && core.isLocalGroupOwner( chat.id() ) && !sent )
      { sent = core.dissolveGroup( chat.id() ); }
    }
    if( phase == "roles" && dissolved ) { timer.stop(); QTimer::singleShot( 1000, &app, [&]() { app.exit( 0 ); } ); }
    if( phase == "rejoin" && (name == "alpha" ? textAck : (received && dissolved)) )
    { timer.stop(); QTimer::singleShot( 1000, &app, [&]() { app.exit( forbidden ? 11 : 0 ); } ); }
  } );
  timer.start( 250 );
  QTimer::singleShot( phase == "resume" ? 360000 : 35000, &app, [&]() { std::fprintf( stderr, "GROUP TIMEOUT %s %s received=%d files=%d textAck=%d fileAck=%d group=%d owner=%d\n", qPrintable( phase ), qPrintable( name ), received, receivedFiles, textAck, fileAck, project().isValid(), core.isLocalGroupOwner( project().id() ) ); app.exit( 3 ); } );
  const int result = app.exec(); core.stop(); settings.save(); app.cleanUp();
  return result;
}

int main( int argc, char** argv )
{
  if( argc > 1 && QByteArray( argv[1] ) == "peer" ) return peer( argc, argv );
  QCoreApplication app( argc, argv ); QTemporaryDir directory;
  if( !directory.isValid() ) return 1;
  QProcessEnvironment environment = QProcessEnvironment::systemEnvironment(); environment.insert( "QT_QPA_PLATFORM", "offscreen" );
  const QString executable = app.applicationFilePath();
  auto phase = [&]( const QString& name, bool offline ) {
    QElapsedTimer duration; duration.start();
    QProcess alpha, beta; alpha.setProcessEnvironment( environment ); beta.setProcessEnvironment( environment );
    const QString first = name == "invite" ? "beta" : "alpha", second = name == "invite" ? "gamma" : "beta";
    if( !offline ) beta.start( executable, QStringList() << "peer" << directory.filePath( second ) << second << name );
    alpha.start( executable, QStringList() << "peer" << directory.filePath( first ) << first << name );
    if( !alpha.waitForFinished( 370000 ) ) { alpha.kill(); alpha.waitForFinished(); }
    if( !offline && !beta.waitForFinished( 370000 ) ) { beta.kill(); beta.waitForFinished(); }
    const bool pass = alpha.exitCode() == 0 && (offline || beta.exitCode() == 0);
    std::printf( "GROUP PHASE %s: %s (alpha %d beta %d, %lld ms)\n", qPrintable( name ), pass ? "PASS" : "FAIL", alpha.exitCode(), beta.exitCode(), duration.elapsed() ); std::fflush( stdout );
    if( !pass ) std::fprintf( stderr, "ALPHA\n%s\nBETA\n%s\n", alpha.readAllStandardError().constData(), beta.readAllStandardError().constData() );
    return pass;
  };
  if( !phase( "seed", false ) ) return 1;
  if( app.arguments().contains( "--invite-only" ) )
  {
    if( !phase( "invite", false ) ) return 1;
    QFile material( directory.filePath( "gamma/received/owner-offline.psd" ) );
    const bool saved = material.open( QIODevice::ReadOnly ) && material.readAll() == "offline-owner-material";
    std::printf( "Offline owner, member invites fresh third peer, bidirectional chat + saved material: %s\n", saved ? "PASS" : "FAIL" );
    return saved ? 0 : 1;
  }
  if( app.arguments().contains( "--images-only" ) )
  {
    if( !phase( "images", false ) ) return 1;
    for( int i = 0; i < 2; ++i )
    {
      QFile source( directory.filePath( QString( "alpha/image-%1.png" ).arg( i ) ) );
      QFile received( directory.filePath( QString( "beta/received/image-%1.png" ).arg( i ) ) );
      if( !source.open( QIODevice::ReadOnly ) || !received.open( QIODevice::ReadOnly ) || source.readAll() != received.readAll() ) return 1;
    }
    std::puts( "Two PNGs actual network byte equality + identical incoming album UUID: PASS" ); return 0;
  }
  if( !phase( "queue", true ) ) return 1;
  QFile queued( directory.filePath( "alpha/pending-transfers.json" ) ); queued.open( QIODevice::ReadOnly );
  const bool persisted = queued.readAll().contains( "materials" ); queued.close();
  std::printf( "Offline material metadata retained after sender restart: %s\n", persisted ? "PASS" : "FAIL" );
  if( !persisted || !phase( "resume", false ) ) return 1;
  QFile material( directory.filePath( "beta/received/materials/sub/asset.psd" ) );
  bool bytes = material.open( QIODevice::ReadOnly ) && material.readAll() == "group-material" &&
    QFileInfo( directory.filePath( "beta/received/materials/sub/empty.txt" ) ).exists() &&
    QFileInfo( directory.filePath( "beta/received/materials/sub/empty.txt" ) ).size() == 0 &&
    QFileInfo( directory.filePath( "beta/received/materials (1)/sub/asset.psd" ) ).exists() &&
    QDir( directory.filePath( "beta/received/materials/empty/subempty" ) ).exists() &&
    QDir( directory.filePath( "beta/received/materials (1)/empty/subempty" ) ).exists();
  foreach( const QString& folder, QStringList() << "materials" << "materials (1)" )
  {
    QFile asset( directory.filePath( "beta/received/" + folder + "/sub/asset.psd" ) );
    QFileInfo empty( directory.filePath( "beta/received/" + folder + "/sub/empty.txt" ) );
    bytes = bytes && asset.open( QIODevice::ReadOnly ) && asset.readAll() == "group-material" && empty.exists() && empty.size() == 0;
    for( int i = 0; i < 501; ++i )
    {
      QFile part( directory.filePath( QString( "beta/received/%1/sub/part-%2.bin" ).arg( folder ).arg( i ) ) );
      bytes = bytes && part.open( QIODevice::ReadOnly ) && part.readAll() == QByteArray::number( i );
    }
  }
  std::printf( "Group directory/empty-file byte verification: %s\n", bytes ? "PASS" : "FAIL" );
  if( !bytes || !phase( "rejoin", false ) || QFile::exists( directory.filePath( "beta/received/must-not-arrive.txt" ) ) ) return 1;
  return phase( "roles", false ) ? 0 : 1;
}
