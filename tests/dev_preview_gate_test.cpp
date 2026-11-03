#include "ShadowDevPreview.h"
#include "Updater.h"
#include <QtCore>
#include <cstdio>

#define CHECK(value) do { if( !(value) ) { std::fprintf( stderr, "FAIL line %d: %s\n", __LINE__, #value ); return 1; } } while(false)

static bool checkArguments( const QStringList& arguments, QString* error )
{
  QList<QByteArray> encoded;
  QVector<char*> pointers;
  for( const QString& argument : arguments )
    encoded.append( argument.toLocal8Bit() );
  for( QByteArray& argument : encoded )
    pointers.append( argument.data() );
  return ShadowDevPreview::checkArguments( pointers.size(), pointers.data(), error );
}

int main( int argc, char** argv )
{
  QCoreApplication app( argc, argv );
  QString error;
  CHECK( Updater::productionManifestUrl() ==
    QUrl( "https://api.github.com/repos/shadowyli/shadow-chat/releases" ) );
  CHECK( checkArguments( QStringList() << "gate-test", &error ) );
  CHECK( ShadowDevPreview::configure( QStringList() << "gate-test", &error, QUrl() ) );

  QTemporaryDir temporary;
  CHECK( temporary.isValid() );
  const QString config = temporary.path() + "/config";
  const QString profile = temporary.path() + "/profile";
  CHECK( QDir().mkpath( config ) && QDir().mkpath( profile ) );
  QFile marker( profile + "/.shadow-dev-preview" );
  CHECK( marker.open( QIODevice::WriteOnly ) );
  CHECK( marker.write( "shadow-native-preview-synthetic-v1\n" ) > 0 );
  marker.close();
  const QStringList arguments = QStringList() << "gate-test" << "--dev-preview" << config
    << "--profile-root" << profile << "--dev-instance" << "12345678-1234-1234-1234-123456789abc";

#if defined(SHADOWCHAT_BUILD_LOCAL) && !defined(SHADOWCHAT_BUILD_MAIN) && !defined(SHADOWCHAT_BUILD_DEV)
  CHECK( checkArguments( arguments, &error ) );
  QStringList duplicate = arguments;
  duplicate << "--dev-instance" << "12345678-1234-1234-1234-123456789abc";
  CHECK( !checkArguments( duplicate, &error ) );
  const QList<QUrl> rejected = QList<QUrl>()
    << QUrl() << Updater::productionManifestUrl()
    << QUrl( "http://localhost:5173/updates/shadowchat.json" )
    << QUrl( "http://127.0.0.1.example.com:5173/updates/shadowchat.json" )
    << QUrl( "http://192.0.2.1:5173/updates/shadowchat.json" )
    << QUrl( "http://[::ffff:127.0.0.1]:5173/updates/shadowchat.json" )
    << QUrl( "ftp://127.0.0.1:5173/updates/shadowchat.json" )
    << QUrl( "http://user:password@127.0.0.1:5173/updates/shadowchat.json" )
    << QUrl( "http://@127.0.0.1:5173/updates/shadowchat.json" )
    << QUrl( "http://127.0.0.1:5173/updates/shadowchat.json?" )
    << QUrl( "http://127.0.0.1:5173/updates/shadowchat.json#fragment" )
    << QUrl( "http://127.0.0.1:5173/updates/other.json" )
    << QUrl( "http://127.0.0.1:5173/updates/shadowchat.json/extra" )
    << QUrl( "http://127.0.0.1:5173/updates%2Fshadowchat.json" )
    << QUrl( "http://127.0.0.1:0/updates/shadowchat.json" )
    << QUrl( "http://127.0.0.1:65536/updates/shadowchat.json" );
  for( const QUrl& source : rejected )
  {
    error.clear();
    if( ShadowDevPreview::configure( arguments, &error, source ) )
    {
      std::fprintf( stderr, "FAIL accepted unsafe source: %s\n", qPrintable( source.toString() ) );
      return 1;
    }
    CHECK( error.contains( "loopback" ) );
    CHECK( !app.property( "shadowDevPreview" ).toBool() );
  }
  CHECK( !ShadowDevPreview::configure( arguments, &error, Updater::productionManifestUrl() ) );
  CHECK( ShadowDevPreview::configure( arguments, &error, QUrl( "https://[::1]:5173/updates/shadowchat.json" ) ) );
  CHECK( ShadowDevPreview::active() );
  app.setProperty( "shadowDevPreview", false );
  CHECK( ShadowDevPreview::configure( arguments, &error, ShadowDevPreview::previewUpdateSource() ) );
  CHECK( ShadowDevPreview::active() );
  CHECK( app.property( "shadowAccountRoot" ).toString() == QFileInfo( profile ).canonicalFilePath() );
  app.setProperty( "shadowDevPreview", false );
  CHECK( marker.remove() );
  CHECK( !ShadowDevPreview::configure( arguments, &error, ShadowDevPreview::previewUpdateSource() ) );
  CHECK( error.contains( "marker" ) && !ShadowDevPreview::active() );
  std::puts( "PASS local preview source, argument and profile gates; unsafe URLs rejected" );
#else
  CHECK( !checkArguments( arguments, &error ) && error.contains( "local build" ) );
  CHECK( !ShadowDevPreview::configure( arguments, &error, ShadowDevPreview::previewUpdateSource() ) );
  CHECK( error.contains( "local build" ) && !ShadowDevPreview::active() );
  CHECK( !ShadowDevPreview::testPeerReady() );
  CHECK( !ShadowDevPreview::sendTestText( 88001, "release must reject local test sender", false, &error ) );
  std::puts( "PASS non-local build rejects preview even with a safe loopback source" );
#endif
  return 0;
}
