#include "Broadcaster.h"
#include "Settings.h"
#include <cstdio>

class BroadcasterProbe : public Broadcaster
{
public:
  BroadcasterProbe() : Broadcaster( Q_NULLPTR ) {}
  using Broadcaster::isNetworkAddressForBroadcast;
};

struct RoutingCase
{
  const char* address;
  quint16 port;
  bool broadcast;
};

int main( int argc, char** argv )
{
  QApplication app( argc, argv );
  BroadcasterProbe broadcaster;
  const quint16 discovery_port = static_cast<quint16>( Settings::instance().defaultBroadcastPort() );
  const quint16 chat_port = static_cast<quint16>( Settings::instance().defaultListenerPort() );
  const RoutingCase cases[] = {
    { "10.255.0.42", chat_port, false },
    { "10.0.255.42", chat_port, false },
    { "10.0.0.255", chat_port, false },
    { "10.0.0.42", chat_port, false },
    { "2001:db8::255", chat_port, false },
    { "10.0.0.7", 0, true },
    { "10.0.0.255", 0, true },
    { "2001:db8::42", 0, true },
    { "10.255.0.42", discovery_port, true },
    { "10.0.0.42", discovery_port, true },
    { "255.255.255.255", discovery_port, true },
    { "ff05::6475", discovery_port, true },
  };
  int failures = 0;
  for( const RoutingCase& test_case : cases )
  {
    const NetworkAddress address( QHostAddress( test_case.address ), test_case.port );
    const bool passed = broadcaster.isNetworkAddressForBroadcast( address ) == test_case.broadcast;
    if( !passed ) ++failures;
    std::printf( "%s %s:%u routes to %s\n", passed ? "PASS" : "FAIL", test_case.address,
      static_cast<unsigned int>( test_case.port ), test_case.broadcast ? "discovery" : "direct TCP" );
  }
  std::puts( "Offline address classification only; NOT real-office-network acceptance." );
  return failures ? 1 : 0;
}
