#include "ShadowStickerStore.h"
#include <QtGui/QImageReader>
#include <QtGui/private/qzipreader_p.h>

namespace {
const qint64 MaxImageBytes = 16 * 1024 * 1024;
const qint64 MaxArchiveBytes = Q_INT64_C(1073741824);
const int MaxEntries = 5000;
bool fail( QString* error, const QString& value ) { if( error ) *error = value; return false; }
bool safePath( const QString& supplied )
{
  QString path = supplied; path.replace( '\\', '/' );
  if( path.isEmpty() || path.startsWith( '/' ) || path.contains( ':' ) || path.contains( QChar( 0 ) ) ) return false;
  foreach( const QString& part, path.split( '/', QString::SkipEmptyParts ) ) if( part == "." || part == ".." ) return false;
  return true;
}
quint32 crc32( const QByteArray& bytes )
{
  quint32 table[256];
  for( quint32 i = 0; i < 256; ++i ) { quint32 value = i; for( int bit = 0; bit < 8; ++bit ) value = (value >> 1) ^ ((value & 1) ? 0xedb88320U : 0U); table[i] = value; }
  quint32 value = 0xffffffffU;
  foreach( unsigned char byte, bytes ) value = table[(value ^ byte) & 255] ^ (value >> 8);
  return value ^ 0xffffffffU;
}
bool imageFormat( const QByteArray& bytes, QString* format, QString* error )
{
  if( bytes.isEmpty() || bytes.size() > MaxImageBytes ) return fail( error, QStringLiteral( "单张表情需小于16MB，且不能是空文件。" ) );
  QBuffer buffer; buffer.setData( bytes ); buffer.open( QIODevice::ReadOnly );
  QImageReader reader( &buffer ); reader.setDecideFormatFromContent( true );
  QString detected = QString::fromLatin1( reader.format() ).toLower();
  if( detected == "jpeg" ) detected = "jpg";
  if( !(QStringList() << "png" << "jpg" << "gif" << "webp").contains( detected ) )
    return fail( error, QStringLiteral( "只支持有效的PNG、JPG、GIF和WebP图片。" ) );
  const QSize size = reader.size();
  if( !size.isValid() || size.width() > 8192 || size.height() > 8192 || qint64( size.width() ) * size.height() > 16 * 1024 * 1024 )
    return fail( error, QStringLiteral( "表情图片尺寸过大或无效。" ) );
  if( reader.read().isNull() ) return fail( error, QStringLiteral( "表情图片损坏，无法读取。" ) );
  *format = detected; return true;
}
}

ShadowStickerStore::ShadowStickerStore( const QString& accountFolder )
  : m_folder( QDir( accountFolder ).absoluteFilePath( "stickers" ) ) {}

bool ShadowStickerStore::load( QList<Sticker>* stickers, QString* error ) const
{
  stickers->clear();
  QFile manifest( QDir( m_folder ).filePath( "index.json" ) );
  if( !manifest.exists() ) return true;
  if( !manifest.open( QIODevice::ReadOnly ) || manifest.size() > 8 * 1024 * 1024 ) return fail( error, QStringLiteral( "无法读取当前账号的表情列表。" ) );
  QJsonParseError parse; QJsonDocument document = QJsonDocument::fromJson( manifest.readAll(), &parse );
  if( parse.error != QJsonParseError::NoError || !document.isObject() || document.object()["version"].toInt() != 1 || !document.object()["items"].isArray() )
    return fail( error, QStringLiteral( "当前账号的表情列表损坏，请先恢复备份。" ) );
  QSet<QString> ids;
  foreach( const QJsonValue& item, document.object()["items"].toArray() )
  {
    const QJsonObject value = item.toObject(); Sticker sticker;
    sticker.id = value["id"].toString(); sticker.format = value["format"].toString(); sticker.name = value["name"].toString();
    if( !QRegularExpression( "^[a-f0-9]{64}$" ).match( sticker.id ).hasMatch() || !(QStringList() << "png" << "jpg" << "gif" << "webp").contains( sticker.format ) || ids.contains( sticker.id ) )
      return fail( error, QStringLiteral( "当前账号的表情列表包含无效记录。" ) );
    ids.insert( sticker.id ); sticker.path = QDir( m_folder ).filePath( sticker.id + "." + sticker.format ); stickers->append( sticker );
  }
  return true;
}

QList<ShadowStickerStore::Sticker> ShadowStickerStore::list() const
{
  QList<Sticker> result; if( !load( &result, Q_NULLPTR ) ) result.clear(); return result;
}

bool ShadowStickerStore::save( const QList<Sticker>& stickers, QString* error ) const
{
  if( !QDir().mkpath( m_folder ) ) return fail( error, QStringLiteral( "无法创建当前账号的表情目录。" ) );
  QJsonArray items;
  foreach( const Sticker& sticker, stickers )
  { QJsonObject item; item["id"] = sticker.id; item["format"] = sticker.format; item["name"] = sticker.name; items.append( item ); }
  QJsonObject object; object["version"] = 1; object["items"] = items;
  QSaveFile file( QDir( m_folder ).filePath( "index.json" ) );
  const QByteArray data = QJsonDocument( object ).toJson( QJsonDocument::Compact );
  if( !file.open( QIODevice::WriteOnly ) || file.write( data ) != data.size() || !file.commit() ) return fail( error, QStringLiteral( "保存表情列表失败，请检查磁盘空间和权限。" ) );
  return true;
}

bool ShadowStickerStore::addImages( const QList<QPair<QString,QByteArray> >& images, QString* error, int* added, int* duplicates )
{
  if( error ) error->clear(); if( added ) *added = 0; if( duplicates ) *duplicates = 0;
  if( !QDir().mkpath( m_folder ) ) return fail( error, QStringLiteral( "无法创建表情目录。" ) );
  QLockFile lock( QDir( m_folder ).filePath( ".write.lock" ) );
  if( !lock.tryLock() ) return fail( error, QStringLiteral( "表情库正在更新，请稍后再试。" ) );
  QList<Sticker> stickers; if( !load( &stickers, error ) ) return false;
  QSet<QString> existing; foreach( const Sticker& sticker, stickers ) existing.insert( sticker.id );
  if( !QDir().mkpath( m_folder ) ) return fail( error, QStringLiteral( "无法创建表情目录。" ) );
  QTemporaryDir stage( QDir( m_folder ).filePath( ".import-XXXXXX" ) );
  if( !stage.isValid() ) return fail( error, QStringLiteral( "无法创建导入临时目录。" ) );
  QList<Sticker> pending; int duplicateCount = 0;
  foreach( const auto& image, images )
  {
    QString format; if( !imageFormat( image.second, &format, error ) ) return false;
    const QString id = QString::fromLatin1( QCryptographicHash::hash( image.second, QCryptographicHash::Sha256 ).toHex() );
    if( existing.contains( id ) ) { ++duplicateCount; continue; }
    Sticker sticker; sticker.id = id; sticker.format = format; sticker.name = QFileInfo( image.first ).completeBaseName();
    sticker.path = QDir( m_folder ).filePath( id + "." + format );
    QFile staged( stage.filePath( id + "." + format ) );
    if( !staged.open( QIODevice::WriteOnly ) || staged.write( image.second ) != image.second.size() ) return fail( error, QStringLiteral( "写入表情失败，请检查磁盘空间。" ) );
    staged.close(); pending.append( sticker ); existing.insert( id );
  }
  if( !installStaged( stage.path(), pending, stickers, error ) ) return false;
  if( added ) *added = pending.size(); if( duplicates ) *duplicates = duplicateCount;
  return true;
}

bool ShadowStickerStore::installStaged( const QString& stage, const QList<Sticker>& pending,
                                       QList<Sticker> stickers, QString* error )
{
  QStringList installed;
  foreach( const Sticker& sticker, pending )
  {
    if( QFile::exists( sticker.path ) || !QFile::rename( QDir( stage ).filePath( sticker.id + "." + sticker.format ), sticker.path ) )
    { foreach( const QString& path, installed ) QFile::remove( path ); return fail( error, QStringLiteral( "无法保存表情，已有同名文件或目录权限不足。" ) ); }
    installed << sticker.path; stickers.append( sticker );
  }
  if( !save( stickers, error ) ) { foreach( const QString& path, installed ) QFile::remove( path ); return false; }
  return true;
}

bool ShadowStickerStore::addFile( const QString& path, QString* error )
{
  QFile file( path );
  if( !file.open( QIODevice::ReadOnly ) || file.size() > MaxImageBytes ) return fail( error, QStringLiteral( "无法读取图片，或图片超过16MB。" ) );
  return addImages( QList<QPair<QString,QByteArray> >() << qMakePair( QFileInfo( path ).fileName(), file.readAll() ), error, Q_NULLPTR, Q_NULLPTR );
}

bool ShadowStickerStore::importZip( const QString& path, QString* error, int* added, int* duplicates,
                                   const std::function<void(int,int)>& progress )
{
  if( error ) error->clear(); if( added ) *added = 0; if( duplicates ) *duplicates = 0;
  QFile archive( path );
  if( !archive.open( QIODevice::ReadOnly ) ) return fail( error, QStringLiteral( "无法读取ZIP，请检查文件权限。" ) );
  if( archive.size() > MaxArchiveBytes ) return fail( error, QStringLiteral( "ZIP压缩包不能超过1GiB（1,073,741,824字节）。" ) );
  if( archive.size() < 22 ) return fail( error, QStringLiteral( "ZIP损坏或为空。" ) );
  QZipReader reader( &archive ); const auto entries = reader.fileInfoList();
  if( reader.status() != QZipReader::NoError || entries.isEmpty() || entries.size() > MaxEntries ) return fail( error, QStringLiteral( "ZIP损坏、为空，或条目超过5000个。" ) );
  qint64 total = 0; QSet<QString> paths;
  foreach( const QZipReader::FileInfo& entry, entries )
  {
    if( !safePath( entry.filePath ) || entry.isSymLink || !entry.isValid() || paths.contains( entry.filePath ) ) return fail( error, QStringLiteral( "ZIP含有不安全路径、链接或重复条目，已拒绝导入。" ) );
    paths.insert( entry.filePath );
    if( entry.size < 0 ) return fail( error, QStringLiteral( "ZIP条目大小无效。" ) );
    if( entry.size > MaxImageBytes ) return fail( error, QStringLiteral( "ZIP单项不能超过16MiB，请拆分过大的表情文件。" ) );
    if( entry.size > MaxArchiveBytes - total ) return fail( error, QStringLiteral( "ZIP解压总量不能超过1GiB（1,073,741,824字节）。" ) );
    total += entry.size;
  }
  if( !QDir().mkpath( m_folder ) ) return fail( error, QStringLiteral( "无法创建表情目录。" ) );
  QLockFile lock( QDir( m_folder ).filePath( ".write.lock" ) );
  if( !lock.tryLock() ) return fail( error, QStringLiteral( "表情库正在更新，请稍后再试。" ) );
  QList<Sticker> stickers;
  if( !load( &stickers, error ) ) return false;
  QSet<QString> existing;
  foreach( const Sticker& sticker, stickers ) existing.insert( sticker.id );
  QTemporaryDir stage( QDir( m_folder ).filePath( ".import-XXXXXX" ) );
  if( !stage.isValid() ) return fail( error, QStringLiteral( "无法创建导入临时目录。" ) );
  QList<Sticker> pending;
  int duplicateCount = 0, processed = 0, imageCount = 0;
  foreach( const QZipReader::FileInfo& entry, entries )
  {
    if( progress ) progress( ++processed, entries.size() );
    if( !entry.isFile ) continue;
    const QString suffix = QFileInfo( entry.filePath ).suffix().toLower();
    if( !(QStringList() << "png" << "jpg" << "jpeg" << "gif" << "webp").contains( suffix ) ) continue;
    QByteArray bytes = reader.fileData( entry.filePath );
    if( reader.status() != QZipReader::NoError || bytes.size() != entry.size || crc32( bytes ) != entry.crc ) return fail( error, QStringLiteral( "ZIP图片损坏或校验失败，未导入任何表情。" ) );
    ++imageCount;
    QString format;
    if( !imageFormat( bytes, &format, error ) ) return false;
    const QString id = QString::fromLatin1( QCryptographicHash::hash( bytes, QCryptographicHash::Sha256 ).toHex() );
    if( existing.contains( id ) ) { ++duplicateCount; continue; }
    Sticker sticker;
    sticker.id = id; sticker.format = format; sticker.name = QFileInfo( entry.filePath ).completeBaseName();
    sticker.path = QDir( m_folder ).filePath( id + "." + format );
    QFile staged( stage.filePath( id + "." + format ) );
    if( !staged.open( QIODevice::WriteOnly ) || staged.write( bytes ) != bytes.size() || !staged.flush() )
      return fail( error, QStringLiteral( "写入表情失败，请检查磁盘空间和权限。" ) );
    staged.close();
    pending.append( sticker ); existing.insert( id );
  }
  if( !imageCount ) return fail( error, QStringLiteral( "压缩包中没有PNG、JPG、GIF或WebP图片。" ) );
  if( !installStaged( stage.path(), pending, stickers, error ) ) return false;
  if( added ) *added = pending.size(); if( duplicates ) *duplicates = duplicateCount;
  return true;
}

bool ShadowStickerStore::remove( const QString& id, QString* error )
{
  QLockFile lock( QDir( m_folder ).filePath( ".write.lock" ) );
  if( !lock.tryLock() ) return fail( error, QStringLiteral( "表情库正在更新，请稍后再试。" ) );
  if( error ) error->clear(); QList<Sticker> stickers; if( !load( &stickers, error ) ) return false;
  for( int i = 0; i < stickers.size(); ++i ) if( stickers[i].id == id )
  {
    const QString path = stickers.takeAt( i ).path;
    if( !save( stickers, error ) ) return false;
    QFile::remove( path ); return true; // Only this library's content-addressed copy.
  }
  return fail( error, QStringLiteral( "未找到这张表情。" ) );
}
