#include "BeeApplication.h"
#include "Core.h"
#include "Protocol.h"
#include "Settings.h"
#include "UserManager.h"
#include "ChatManager.h"
#include "IconManager.h"
#include "Random.h"
#include <cstdio>

class DeferredCore : public Core
{
public:
  using Core::parseGroupMessage;
};

static User peer( VNumber id, bool verified )
{
  User user( id );
  user.setName( QString( "synthetic-peer-%1" ).arg( id ) );
  user.setAccountName( QString( "peer-%1@example.test" ).arg( id ) );
  user.setHash( QString( "synthetic-hash-%1" ).arg( id ) );
  user.setProtocolVersion( Settings::instance().protocolVersion() );
  user.setStatus( verified ? User::Online : User::Offline );
  NetworkAddress address( QHostAddress::LocalHost, 47650 );
  if( verified ) address.setInfo( Protocol::instance().shadowChatIdentityMarker() );
  user.setNetworkAddress( address );
  return user;
}

static void settle()
{
  for( int iteration = 0; iteration < 4; ++iteration ) QApplication::processEvents();
}

int main( int argc, char** argv )
{
  BeeApplication app( argc, argv );
  QTemporaryDir directory;
  Random::init();
  Settings::instance().setDefaultFolders();
  Settings::instance().createLocalUser( "deferred-group-fixture" );
  Settings::instance().setAccountDataFolder( directory.path() );
  Settings::instance().load();
  IconManager::instance();
  DeferredCore core;
  int failures = 0;
  for( int scenario = 0; scenario < 3; ++scenario )
  {
    const User sender = peer( 97101, true );
    User unknown = peer( 97102, false );
    UserManager::instance().setUser( sender );
    UserManager::instance().setUser( unknown );
    Group group;
    group.setId( 97300+scenario );
    group.setName( QString( "deferred-group-%1" ).arg( scenario ) );
    group.setPrivateId( QString( "deferred-identity-%1" ).arg( scenario ) );
    group.setChatType( Group::GroupChat );
    group.setOwnerHash( sender.hash() );
    group.setLastModified( QDateTime::currentDateTime() );
    group.addUser( ID_LOCAL_USER );
    group.addUser( sender.id() );
    group.addUser( unknown.id() );
    Chat full;
    full.setGroup( group );
    const Message request = Protocol::instance().groupChatRequestMessage( full, Settings::instance().localUser() );
    core.parseGroupMessage( sender, request );
    Chat current = ChatManager::instance().findChatByPrivateId( group.privateId(), true, ID_INVALID );
    if( !current.isValid() || current.hasUser( unknown.id() ) ) { ++failures; continue; }
    if( scenario == 1 )
    {
      Group changed = current.group();
      changed.setName( "newer-group-name" );
      if( !core.changeGroupChat( sender, changed ) ) ++failures;
    }
    if( scenario == 2 )
    {
      core.markGroupDissolved( group.privateId() );
      core.removeChat( current.id(), true );
    }
    unknown = peer( unknown.id(), true );
    UserManager::instance().setUser( unknown );
    core.userChanged( unknown );
    settle();
    current = ChatManager::instance().findChatByPrivateId( group.privateId(), true, ID_INVALID );
    const bool passed = scenario == 0 ? current.isValid() && current.hasUser( unknown.id() ) :
      scenario == 1 ? current.isValid() && current.name() == "newer-group-name" && !current.hasUser( unknown.id() ) :
      !current.isValid();
    if( !passed ) ++failures;
    std::printf( "%s deferred-group scenario%d (resolve, stale rename, dissolved)\n", passed ? "PASS" : "FAIL", scenario );
  }
  return failures ? 1 : 0;
}
