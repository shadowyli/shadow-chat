#include "BeeApplication.h"
#include "Core.h"
#include "Settings.h"
#include "UserManager.h"
#include "Protocol.h"
#include "IconManager.h"
#include "Random.h"
#include "ShadowWorkgroups.h"
#include "ShadowDirectoryPage.h"
#include <QtTest>
#include <cstdio>

#define CHECK(value) do { if( !(value) ) { std::fprintf( stderr, "FAIL line %d: %s\n", __LINE__, #value ); return 1; } } while( false )

int main( int argc, char** argv )
{
  BeeApplication app( argc, argv );
  QTemporaryDir fixture;
  CHECK( fixture.isValid() );
  app.setProperty( "shadowAccountRoot", fixture.path() );
  Random::init();
  Settings& settings = Settings::instance();
  settings.setDefaultFolders();
  settings.createLocalUser( "synthetic-workgroup-profile" );
  settings.setAccountDataFolder( fixture.path() );
  settings.load();
  User local = settings.localUser();
  local.setAccountName( "local-workgroups@example.test" );
  local.setWorkgroups( QStringList{ QStringLiteral( "设计组" ) } );
  VCard card = local.vCard();
  card.setNickName( "synthetic-workgroup-profile" );
  card.setInfo( "synthetic-profile-only" );
  local.setVCard( card );
  settings.setLocalUser( local );
  settings.setAcceptConnectionsOnlyFromWorkgroups( false );
  IconManager::instance();
  Core core;
  QStringList normalized;
  CHECK( ShadowWorkgroups::fromText( QStringLiteral( " 设计组，程序组、设计组 " ), &normalized ) &&
    normalized == QStringList( { QStringLiteral( "设计组" ), QStringLiteral( "程序组" ) } ) );
  CHECK( !ShadowWorkgroups::fromJson( "[5]", &normalized ) &&
    !ShadowWorkgroups::fromJson( "[\"bad\\u2028separator\"]", &normalized ) &&
    !ShadowWorkgroups::fromText( QString( 129, QLatin1Char( 'A' ) ), &normalized ) );
  User peer( 60001 );
  peer.setName( "synthetic-remote" );
  peer.setAccountName( "remote-workgroups@example.test" );
  peer.setHash( "synthetic-verified-workgroup-peer" );
  peer.setProtocolVersion( settings.protocolVersion() );
  NetworkAddress address( QHostAddress::LocalHost, 47681 );
  address.setInfo( Protocol::instance().shadowChatIdentityMarker() );
  peer.setNetworkAddress( address );
  peer.setStatus( User::Online );
  peer.setWorkgroups( QStringList{ QStringLiteral( "原工作组" ) } );
  UserManager::instance().setUser( peer );
  const QString identity = peer.hash();
  const Message updated = Protocol::instance().localVCardMessage( peer.protocolVersion() );
  User changed = peer;
  CHECK( Protocol::instance().changeVCardFromMessage( &changed, updated ) &&
    changed.workgroups() == local.workgroups() && changed.hash() == identity );
  User legacy = peer;
  QStringList base = updated.data().split( QChar::LineSeparator );
  base.removeLast();
  base.removeLast();
  Message oldMessage = updated;
  oldMessage.setData( base.join( QChar::LineSeparator ) );
  CHECK( Protocol::instance().changeVCardFromMessage( &legacy, oldMessage ) &&
    legacy.workgroups() == peer.workgroups() );
  Message malformed = updated;
  QStringList bad = updated.data().split( QChar::LineSeparator );
  bad.last() = "[5]";
  malformed.setData( bad.join( QChar::LineSeparator ) );
  User unchanged = peer;
  CHECK( !Protocol::instance().changeVCardFromMessage( &unchanged, malformed ) &&
    unchanged.workgroups() == peer.workgroups() && unchanged.vCard() == peer.vCard() );
  bool notified = false;
  QObject::connect( &core, &Core::userChanged, &app, [&]( const User& user ) {
    if( user.id() == peer.id() && user.workgroups() == local.workgroups() ) notified = true;
  } );
  CHECK( QMetaObject::invokeMethod( &core, "parseMessage", Qt::DirectConnection,
    Q_ARG( User, peer ), Q_ARG( Message, updated ) ) );
  CHECK( notified && UserManager::instance().findUser( peer.id() ).workgroups() == local.workgroups() );
  ShadowDirectoryPage directory;
  directory.setSection( ShadowDirectoryPage::Organization );
  directory.resize( 900, 600 );
  directory.show();
  QTest::qWait( 25 );
  QTreeWidget* listing = directory.findChild<QTreeWidget*>( "ShadowDirectoryList" );
  CHECK( listing && listing->topLevelItemCount() == 2 &&
    listing->topLevelItem( 0 )->text( 0 ) == QStringLiteral( "设计组" ) );
  CHECK( directory.findChild<QPushButton*>( "ShadowEditOwnWorkgroups" )->isVisible() );
  changed = UserManager::instance().findUser( peer.id() );
  changed.setWorkgroups( QStringList{ QStringLiteral( "程序组" ) } );
  UserManager::instance().setUser( changed );
  directory.refresh();
  CHECK( listing->topLevelItem( 0 )->text( 0 ) == QStringLiteral( "程序组" ) );
  const QString proof = qEnvironmentVariable( "SHADOWCHAT_UI_PROOF_DIR", QDir::currentPath() );
  QDir().mkpath( proof );
  CHECK( directory.grab().save( QDir( proof ).filePath( "organization-workgroups.png" ) ) );
  core.setLocalUserWorkgroups( QStringList{ QStringLiteral( "美术组" ) } );
  CHECK( !settings.acceptConnectionsOnlyFromWorkgroups() &&
    settings.localUser().workgroups() == QStringList( { QStringLiteral( "美术组" ) } ) );
  QSettings stored( QDir( fixture.path() ).filePath( "beebeep.ini" ), QSettings::IniFormat );
  CHECK( stored.value( "Network/Workgroups" ).toStringList() == settings.localUser().workgroups() );
  settings.setAcceptConnectionsOnlyFromWorkgroups( true );
  core.setLocalUserWorkgroups( QStringList{ QStringLiteral( "维护组" ) } );
  CHECK( settings.acceptConnectionsOnlyFromWorkgroups() );
  std::puts( "PASS workgroups normalize, persist, update verified peers and regroup the directory without changing network policy" );
  std::puts( "PASS old profile messages preserve existing groups and malformed extensions never alter peers" );
  return 0;
}
