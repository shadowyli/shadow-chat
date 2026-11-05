#include "ShadowGitHubRelease.h"
#include "ShadowGitHubFallback.h"
#include <QCoreApplication>
#include <QSaveFile>
#include <QTimer>
#include <cstdio>

static QByteArray readFile( const QString& path )
{
  QFile file( path );
  return file.open( QIODevice::ReadOnly ) ? file.readAll() : QByteArray();
}

static int report( const QJsonObject& object, const QString& output = QString() )
{
  const QByteArray bytes = QJsonDocument( object ).toJson();
  std::fwrite( bytes.constData(), 1, size_t( bytes.size() ), stdout );
  if( output.isEmpty() ) return 0;
  QSaveFile file( output );
  return file.open( QIODevice::WriteOnly ) && file.write( bytes ) == bytes.size() && file.commit() ? 0 : 2;
}

int main( int argc, char** argv )
{
  QCoreApplication app( argc, argv );
  const QStringList arguments = app.arguments();
  if( arguments.size() != 4 ) return 2;
  const QUrl source( "https://api.github.com/repos/shadowyli/shadow-chat/releases" );
  if( arguments.at( 1 ) == "validate" )
  {
    const QString folder = arguments.at( 2 );
    const QString version = arguments.at( 3 );
    const QByteArray metadata = readFile( QDir( folder ).filePath( "build-info.json" ) );
    const QJsonObject fields = QJsonDocument::fromJson( metadata ).object();
    ShadowGitHubRelease::FeedRelease release;
    release.tag = "v" + version + "-test";
    release.version = version;
    ShadowGitHubRelease::MetadataStatus status;
    const QByteArray manifest = ShadowGitHubRelease::metadataManifest( metadata, release, "test", "local", source, &status );
    const QString name = QString( "ShadowChat-%1-local-setup.exe" ).arg( version );
    const auto checksum = ShadowGitHubRelease::checksums( readFile( QDir( folder ).filePath( "SHA256SUMS.txt" ) ),
      name, fields.value( "installer" ).toObject().value( "sha256" ).toString(), metadata );
    QString parsedVersion, download;
    const bool valid = status == ShadowGitHubRelease::MetadataStatus::Valid &&
      checksum == ShadowGitHubRelease::ChecksumStatus::Match &&
      ShadowUpdateManifest::parse( manifest, source, &parsedVersion, &download, "test" ) &&
      parsedVersion == version;
    report( QJsonObject{ {"passed", valid}, {"mode", "legacy-metadata-and-checksum"},
      {"version", parsedVersion}, {"downloadUrl", download}, {"consumer", "unchanged-1.2.1-headers"} } );
    return valid ? 0 : 1;
  }
  if( arguments.at( 1 ) != "live" ) return 2;
  const QString expected = arguments.at( 2 );
  const QString output = arguments.at( 3 );
  bool passed = false;
  int reportResult = 0;
  ShadowGitHubFallback* fallback = new ShadowGitHubFallback( &app, source, "test", "local", 60000,
    [&]( const QByteArray& manifest, const QString& error ) {
      QString version, download;
      passed = error.isEmpty() && ShadowUpdateManifest::parse( manifest, source, &version, &download, "test" ) &&
        version == expected && QVersionNumber::compare( QVersionNumber::fromString( version ),
          QVersionNumber::fromString( "1.2.1" ) ) > 0;
      reportResult = report( QJsonObject{ {"passed", passed}, {"mode", "live-fallback-without-api"},
        {"version", version}, {"downloadUrl", download}, {"error", error},
        {"consumer", "unchanged-1.2.1-headers"}, {"source", source.toString()} }, output );
      app.quit();
    } );
  QTimer::singleShot( 0, &app, [fallback]() { fallback->start(); } );
  QTimer::singleShot( 65000, &app, &QCoreApplication::quit );
  app.exec();
  return passed && reportResult == 0 ? 0 : 1;
}
