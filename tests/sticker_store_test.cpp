#include "ShadowStickerStore.h"
#include <QImage>
#include <QtGui/private/qzipwriter_p.h>
#include <cstdio>

static QByteArray png()
{
  QImage image( 32, 24, QImage::Format_ARGB32 ); image.fill( QColor( "#3479e2" ) );
  QByteArray bytes; QBuffer buffer( &bytes ); buffer.open( QIODevice::WriteOnly ); image.save( &buffer, "PNG" ); return bytes;
}
static void write( const QString& path, const QByteArray& data )
{ QFile file( path ); if( !file.open( QIODevice::WriteOnly ) || file.write( data ) != data.size() ) qFatal( "fixture write failed" ); }
static QByteArray read( const QString& path )
{ QFile file( path ); return file.open( QIODevice::ReadOnly ) ? file.readAll() : QByteArray(); }
static void zip( const QString& path, const QList<QPair<QString,QByteArray> >& files )
{ QZipWriter writer( path ); writer.setCompressionPolicy( QZipWriter::NeverCompress ); foreach( const auto& file, files ) writer.addFile( file.first, file.second ); writer.close(); }

int main( int argc, char** argv )
{
  QCoreApplication app( argc, argv ); QTemporaryDir root; std::setvbuf( stdout, Q_NULLPTR, _IONBF, 0 );
  int failures = 0; auto check = [&]( bool pass, const char* label ) { std::printf( "%s %s\n", pass ? "PASS" : "FAIL", label ); if( !pass ) ++failures; };
  ShadowStickerStore a( root.filePath( "account-a" ) ), b( root.filePath( "account-b" ) );
  const QByteArray bytes = png(); const QString source = root.filePath( "original.png" ); write( source, bytes ); QString error;
  check( a.addFile( source, &error ) && a.list().size() == 1, "favorite imports a valid original image" );
  const auto item = a.list().first();
  check( read( item.path ) == bytes && item.id == QString::fromLatin1( QCryptographicHash::hash( bytes, QCryptographicHash::Sha256 ).toHex() ), "original bytes preserved under SHA256 identity" );
  check( b.list().isEmpty(), "account libraries are isolated" );
  check( a.addFile( source, &error ) && a.list().size() == 1, "favorite content deduplicates" );
  ShadowStickerStore restart( root.filePath( "account-a" ) );
  check( restart.list().size() == 1 && restart.list().first().path == item.path, "library persists across store restart" );
  check( restart.remove( item.id, &error ) && restart.list().isEmpty() && !QFile::exists( item.path ) && read( source ) == bytes, "remove deletes only library copy, preserves chat original" );
  const QString archive = root.filePath( "valid.zip" );
  zip( archive, { qMakePair( QString( "favorites/one.png" ), bytes ), qMakePair( QString( "favorites/two.png" ), bytes ), qMakePair( QString( "manifest.xml" ), QByteArray( "metadata only" ) ) } );
  int added = 0, duplicates = 0;
  check( a.importZip( archive, &error, &added, &duplicates ) && added == 1 && duplicates == 1 && a.list().size() == 1, "ZIP import skips XML and deduplicates repeated image content" );
  check( a.importZip( archive, &error, &added, &duplicates ) && added == 0 && duplicates == 2, "reimport ZIP is idempotent" );
  const int before = a.list().size();
  const QString traversal = root.filePath( "traversal.zip" ); zip( traversal, { qMakePair( QString( "../escape.png" ), bytes ) } );
  check( !a.importZip( traversal, &error ) && !QFile::exists( root.filePath( "escape.png" ) ) && a.list().size() == before, "path traversal rejects entire import without changes" );
  const QString absolute = root.filePath( "absolute.zip" ); zip( absolute, { qMakePair( QString( "C:/escape.png" ), bytes ) } );
  check( !a.importZip( absolute, &error ), "drive absolute path is rejected" );
  const QString link = root.filePath( "link.zip" ); { QZipWriter writer( link ); writer.addSymLink( "unsafe.png", "../original.png" ); writer.close(); }
  check( !a.importZip( link, &error ), "ZIP symlink is rejected" );
  const QString damaged = root.filePath( "damaged.zip" ); QByteArray damagedBytes = read( archive );
  const int imageAt = damagedBytes.indexOf( bytes ); if( imageAt < 0 ) return 2; damagedBytes[imageAt + bytes.size() / 2] = char( damagedBytes.at( imageAt + bytes.size() / 2 ) ^ 0x1 );
  write( damaged, damagedBytes );
  check( !a.importZip( damaged, &error ) && a.list().size() == before, "CRC damage rejects import without altering library" );
  const QString invalid = root.filePath( "invalid.zip" ); zip( invalid, { qMakePair( QString( "valid.png" ), bytes ), qMakePair( QString( "broken.png" ), QByteArray( "not an image" ) ) } );
  check( !b.importZip( invalid, &error ) && b.list().isEmpty(), "invalid image rolls back entire batch" );
  const QString huge = root.filePath( "huge.zip" ); zip( huge, { qMakePair( QString( "big.png" ), QByteArray( 17 * 1024 * 1024, 'x' ) ) } );
  check( !a.importZip( huge, &error ), "oversized image entry is rejected before decoding" );
  const QString many = root.filePath( "many.zip" ); { QZipWriter writer( many ); for( int i = 0; i < 5001; ++i ) writer.addDirectory( QString::number( i ) ); writer.close(); }
  check( !a.importZip( many, &error ), "archive entry count is bounded" );
  const QString manifest = root.filePath( "account-b/stickers/index.json" ); QDir().mkpath( QFileInfo( manifest ).absolutePath() ); write( manifest, "broken-json" );
  check( !b.addFile( source, &error ) && read( manifest ) == "broken-json", "corrupt existing manifest is never overwritten" );
  const QByteArray gif = QByteArray::fromBase64( "R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7" );
  const QString gifPath = root.filePath( "animation.gif" ); write( gifPath, gif );
  check( a.addFile( gifPath, &error ) && read( a.list().last().path ) == gif, "GIF bytes are preserved without re-encoding" );
  QImage image( 20, 20, QImage::Format_RGB32 ); image.fill( Qt::green );
  const QString jpeg = root.filePath( "photo.jpg" ), webp = root.filePath( "photo.webp" );
  check( image.save( jpeg, "JPG" ) && a.addFile( jpeg, &error ), "JPG is supported" );
  check( image.save( webp, "WEBP" ) && a.addFile( webp, &error ), "WebP is supported" );
  const QString provided = qEnvironmentVariable( "SHADOW_STICKER_TEST_ZIP" );
  if( !provided.isEmpty() )
  {
    ShadowStickerStore external( root.filePath( "provided-zip-test-only" ) ); QElapsedTimer elapsed; elapsed.start();
    const bool imported = external.importZip( provided, &error, &added, &duplicates );
    std::printf( "Provided external ZIP: added=%d duplicates=%d milliseconds=%lld error=%s\n", added, duplicates, elapsed.elapsed(), qPrintable( error ) );
    check( imported && added == 513 && duplicates == 10 && added + duplicates == 523 && external.list().size() == 513, "provided POPO ZIP validates all523 PNGs and stores513 unique originals; testtemp only" );
  }
  return failures ? 1 : 0;
}
