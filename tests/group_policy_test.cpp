#include "ShadowGroupPolicy.h"
#include <QCoreApplication>
int main( int argc, char** argv )
{
  QCoreApplication app( argc, argv );
  int failures = 0;
  auto check = [&]( bool ok, const char* label ) { if( !ok ) { qCritical() << label; ++failures; } };
  Group original;
  original.setId( 1 ); original.setChatType( Group::GroupChat );
  original.setName( "Art" ); original.setOwnerHash( "owner" );
  original.addUser( 10 ); original.addUser( 20 );
  Group invited = original; invited.addUser( 30 );
  check( ShadowGroupPolicy::canChange( original, invited, 20, "member" ), "member may invite while owner offline" );
  Group renamed = original; renamed.setName( "Renamed" );
  check( !ShadowGroupPolicy::canChange( original, renamed, 20, "member" ), "member rename denied" );
  check( ShadowGroupPolicy::canChange( original, renamed, 10, "owner" ), "owner rename allowed" );
  Group removed = original; removed.removeUser( 10 );
  check( !ShadowGroupPolicy::canChange( original, removed, 20, "member" ), "member removal denied" );
  Group transferred = original; transferred.setOwnerHash( "member" );
  check( !ShadowGroupPolicy::canChange( original, transferred, 20, "member" ), "member cannot take ownership" );
  check( ShadowGroupPolicy::canChange( original, transferred, 10, "owner" ), "owner transfer allowed" );
  check( !ShadowGroupPolicy::canChange( original, invited, 99, "owner" ), "nonmember owner spoof denied" );
  check( invited.ownerHash() == "owner", "copy preserves owner identity" );
  return failures ? 1 : 0;
}
