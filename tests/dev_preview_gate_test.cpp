#include "ShadowDevPreview.h"
#include <cstdio>

int main()
{
  char executable[] = "gate-test";
  char flag[] = "--dev-preview";
  char path[] = "C:/synthetic/config";
  char* arguments[] = { executable, flag, path };
  QString error;
  if( ShadowDevPreview::checkArguments( 3, arguments, &error ) || !error.contains( "local build" ) )
  {
    std::fprintf( stderr, "FAIL main/dev release accepted preview argument\n" );
    return 1;
  }
  if( !ShadowDevPreview::checkArguments( 1, arguments, &error ) )
    return 1;
  if( ShadowDevPreview::configure( QStringList() << executable << flag << path, &error ) || !error.contains( "loopback" ) )
  {
    std::fprintf( stderr, "FAIL preview configure accepted a non-loopback production update source\n" );
    return 1;
  }
  std::puts( "PASS release arguments rejected, normal arguments accepted, non-loopback production update source rejected" );
  return 0;
}
