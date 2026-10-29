#include "ShadowMessageDelivery.h"
int main( int argc, char** argv )
{
  QCoreApplication app( argc, argv ); QTemporaryDir directory;
  if( !directory.isValid() ) return 1;
  int failures = 0;
  auto check = [&]( bool ok, const char* label ) { if( !ok ) { qCritical() << label; ++failures; } };
  QDir root( directory.path() ); root.mkpath( "a" ); root.mkpath( "b" );
  ShadowMessageDelivery ledger;
  check( ledger.open( root.filePath( "a" ) ), "open ledger" );
  check( ledger.begin( "42", "group", QStringList() << "a" << "b" << "c" << "d" << "e", "material" ), "register five recipients" );
  check( !ledger.acknowledge( "42", "outsider" ).changed, "foreign recipient cannot acknowledge" );
  check( ledger.acknowledge( "42", "a" ).delivered == 1, "first delivery" );
  check( !ledger.acknowledge( "42", "a" ).changed, "duplicate acknowledgement ignored" );
  ledger.acknowledge( "42", "b" );
  const auto third = ledger.acknowledge( "42", "c" );
  check( third.changed && third.delivered == 3 && third.total == 5, "three of five reflects actual acknowledgements" );
  ShadowMessageDelivery restored;
  check( restored.open( root.filePath( "a" ) ), "restart ledger" );
  check( !restored.acknowledge( "42", "a" ).changed, "delivered recipients retained across restart" );
  restored.cancel( "group", "e" );
  const auto last = restored.acknowledge( "42", "d" );
  check( last.changed && last.delivered == 4 && last.total == 4 && last.chat == "group", "departed recipient removed from pending count" );
  check( !restored.acknowledge( "42", "d" ).changed, "completed ledger entry retired" );
  restored.begin( "43", "group", QStringList() << "a", "pending" );
  restored.cancel( "group" );
  check( !restored.acknowledge( "43", "a" ).changed, "leaving group cancels pending delivery" );
  check( restored.open( root.filePath( "b" ) ) && !restored.acknowledge( "42", "a" ).changed, "accounts isolated" );
  return failures ? 1 : 0;
}
