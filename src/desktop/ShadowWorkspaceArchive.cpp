//////////////////////////////////////////////////////////////////////
//
// 影聊 portable workspace archive.
//
//////////////////////////////////////////////////////////////////////

#include "ShadowWorkspaceArchive.h"

#include <QCryptographicHash>
#include <QSaveFile>


namespace
{
  const quint32 ShadowWorkspaceMagic = 0x53435753; // SCWS
  const quint16 ShadowWorkspaceVersion = 1;
  const quint32 ShadowWorkspaceMaxEntries = 8;
  const quint64 ShadowWorkspaceMaxEntrySize = 128ULL * 1024ULL * 1024ULL;
  const quint64 ShadowWorkspaceMaxTotalSize = 256ULL * 1024ULL * 1024ULL;
  const quint64 ShadowWorkspaceMaxArchiveSize = 260ULL * 1024ULL * 1024ULL;

  void setError( QString* error_message, const QString& message )
  {
    if( error_message )
      *error_message = message;
  }

  quint32 compressedDeclaredSize( const QByteArray& compressed )
  {
    if( compressed.size() < 4 )
      return 0;
    const uchar* bytes = reinterpret_cast<const uchar*>( compressed.constData() );
    return (static_cast<quint32>( bytes[0] ) << 24) |
           (static_cast<quint32>( bytes[1] ) << 16) |
           (static_cast<quint32>( bytes[2] ) << 8) |
           static_cast<quint32>( bytes[3] );
  }
}

QString ShadowWorkspaceArchive::fileExtension()
{
  return QStringLiteral( "beebeep-workspace" );
}

QString ShadowWorkspaceArchive::pendingFilePath( const QString& data_folder )
{
  return QDir( data_folder ).filePath( QStringLiteral( ".shadowchat-workspace-pending" ) );
}

QStringList ShadowWorkspaceArchive::allowedEntryNames()
{
  return QStringList() << QStringLiteral( "beebeep.ini" )
                       << QStringLiteral( "beebeep.dat" )
                       << QStringLiteral( "beebeep.off" )
                       << QStringLiteral( "beehosts.ini" );
}

bool ShadowWorkspaceArchive::exportWorkspace( const QString& data_folder, const QString& archive_path, QString* error_message )
{
  QList<Entry> entries;
  foreach( const QString& file_name, allowedEntryNames() )
  {
    QFile file( QDir( data_folder ).filePath( file_name ) );
    if( !file.exists() )
      continue;
    if( !file.open( QIODevice::ReadOnly ) )
    {
      setError( error_message, QStringLiteral( "无法读取 %1。" ).arg( file_name ) );
      return false;
    }
    if( static_cast<quint64>( file.size() ) > ShadowWorkspaceMaxEntrySize )
    {
      setError( error_message, QStringLiteral( "%1 过大，无法加入工作区。" ).arg( file_name ) );
      return false;
    }
    Entry entry;
    entry.name = file_name;
    entry.data = file.readAll();
    entries.append( entry );
  }

  if( entries.isEmpty() )
  {
    setError( error_message, QStringLiteral( "当前没有可导出的工作区数据。" ) );
    return false;
  }

  QSaveFile archive( archive_path );
  if( !archive.open( QIODevice::WriteOnly ) )
  {
    setError( error_message, QStringLiteral( "无法创建工作区文件，请检查保存位置。" ) );
    return false;
  }

  QDataStream stream( &archive );
  stream.setVersion( QDataStream::Qt_5_6 );
  stream << ShadowWorkspaceMagic << ShadowWorkspaceVersion << QDateTime::currentDateTimeUtc();
  stream << static_cast<quint32>( entries.size() );
  foreach( const Entry& entry, entries )
  {
    const QByteArray digest = QCryptographicHash::hash( entry.data, QCryptographicHash::Sha256 );
    const QByteArray compressed = qCompress( entry.data, 9 );
    stream << entry.name << static_cast<quint64>( entry.data.size() ) << digest << compressed;
  }

  if( stream.status() != QDataStream::Ok || !archive.commit() )
  {
    setError( error_message, QStringLiteral( "写入工作区文件失败。" ) );
    return false;
  }
  return true;
}

bool ShadowWorkspaceArchive::readArchive( const QString& archive_path, QList<Entry>* entries, QString* error_message )
{
  if( !entries )
  {
    setError( error_message, QStringLiteral( "工作区读取参数无效。" ) );
    return false;
  }
  entries->clear();

  const QFileInfo archive_info( archive_path );
  if( !archive_info.exists() || archive_info.size() <= 0 || static_cast<quint64>( archive_info.size() ) > ShadowWorkspaceMaxArchiveSize )
  {
    setError( error_message, QStringLiteral( "工作区文件大小异常。" ) );
    return false;
  }

  QFile archive( archive_path );
  if( !archive.open( QIODevice::ReadOnly ) )
  {
    setError( error_message, QStringLiteral( "无法打开工作区文件。" ) );
    return false;
  }

  QDataStream stream( &archive );
  stream.setVersion( QDataStream::Qt_5_6 );
  quint32 magic = 0;
  quint16 version = 0;
  QDateTime created_at;
  quint32 entry_count = 0;
  stream >> magic >> version >> created_at >> entry_count;
  if( magic != ShadowWorkspaceMagic || version != ShadowWorkspaceVersion || entry_count == 0 || entry_count > ShadowWorkspaceMaxEntries )
  {
    setError( error_message, QStringLiteral( "这不是受支持的影聊工作区文件。" ) );
    return false;
  }

  const QStringList allowed_names = allowedEntryNames();
  QSet<QString> names_seen;
  quint64 total_size = 0;
  for( quint32 i = 0; i < entry_count; ++i )
  {
    QString name;
    quint64 original_size = 0;
    QByteArray expected_digest;
    QByteArray compressed;
    stream >> name >> original_size >> expected_digest >> compressed;
    if( stream.status() != QDataStream::Ok || !allowed_names.contains( name ) || names_seen.contains( name ) ||
        QFileInfo( name ).fileName() != name || name.contains( QLatin1String( ".." ) ) )
    {
      setError( error_message, QStringLiteral( "工作区目录不合法或文件已损坏。" ) );
      return false;
    }
    if( original_size > ShadowWorkspaceMaxEntrySize || total_size + original_size > ShadowWorkspaceMaxTotalSize ||
        compressedDeclaredSize( compressed ) != original_size )
    {
      setError( error_message, QStringLiteral( "工作区数据大小异常。" ) );
      return false;
    }

    const QByteArray data = qUncompress( compressed );
    if( static_cast<quint64>( data.size() ) != original_size ||
        QCryptographicHash::hash( data, QCryptographicHash::Sha256 ) != expected_digest )
    {
      setError( error_message, QStringLiteral( "工作区完整性校验失败。" ) );
      return false;
    }

    Entry entry;
    entry.name = name;
    entry.data = data;
    entries->append( entry );
    names_seen.insert( name );
    total_size += original_size;
  }

  if( stream.status() != QDataStream::Ok || !archive.atEnd() )
  {
    setError( error_message, QStringLiteral( "工作区文件尾部异常。" ) );
    return false;
  }
  return true;
}

bool ShadowWorkspaceArchive::stageImport( const QString& data_folder, const QString& archive_path, QString* error_message )
{
  QList<Entry> entries;
  if( !readArchive( archive_path, &entries, error_message ) )
    return false;
  Q_UNUSED( entries )

  if( !QDir().mkpath( data_folder ) )
  {
    setError( error_message, QStringLiteral( "无法创建影聊数据目录。" ) );
    return false;
  }

  QFile source( archive_path );
  if( !source.open( QIODevice::ReadOnly ) )
  {
    setError( error_message, QStringLiteral( "无法再次读取工作区文件。" ) );
    return false;
  }
  QSaveFile pending( pendingFilePath( data_folder ) );
  if( !pending.open( QIODevice::WriteOnly ) || pending.write( source.readAll() ) != source.size() || !pending.commit() )
  {
    setError( error_message, QStringLiteral( "无法暂存工作区，请检查数据目录权限。" ) );
    return false;
  }
  return true;
}

bool ShadowWorkspaceArchive::applyPendingImport( const QString& data_folder, QString* restore_folder, QString* error_message )
{
  const QString pending_path = pendingFilePath( data_folder );
  if( !QFileInfo::exists( pending_path ) )
    return true;

  QList<Entry> entries;
  if( !readArchive( pending_path, &entries, error_message ) )
    return false;

  QDir data_dir( data_folder );
  const QString restore_name = QStringLiteral( "workspace-restore-%1" ).arg( QDateTime::currentDateTime().toString( QStringLiteral( "yyyyMMdd-HHmmss" ) ) );
  const QString restore_path = data_dir.filePath( restore_name );
  if( !QDir().mkpath( restore_path ) )
  {
    setError( error_message, QStringLiteral( "无法创建导入前恢复点。" ) );
    return false;
  }

  const QStringList allowed_names = allowedEntryNames();
  QSet<QString> originally_present;
  foreach( const QString& name, allowed_names )
  {
    const QString current_path = data_dir.filePath( name );
    if( QFileInfo::exists( current_path ) )
    {
      originally_present.insert( name );
      if( !QFile::copy( current_path, QDir( restore_path ).filePath( name ) ) )
      {
        setError( error_message, QStringLiteral( "创建恢复点时无法复制 %1。" ).arg( name ) );
        return false;
      }
    }
  }

  bool write_ok = true;
  foreach( const QString& name, allowed_names )
    QFile::remove( data_dir.filePath( name ) );
  foreach( const Entry& entry, entries )
  {
    QSaveFile target( data_dir.filePath( entry.name ) );
    if( !target.open( QIODevice::WriteOnly ) || target.write( entry.data ) != entry.data.size() || !target.commit() )
    {
      write_ok = false;
      break;
    }
  }

  if( !write_ok )
  {
    foreach( const QString& name, allowed_names )
      QFile::remove( data_dir.filePath( name ) );
    foreach( const QString& name, originally_present )
      QFile::copy( QDir( restore_path ).filePath( name ), data_dir.filePath( name ) );
    setError( error_message, QStringLiteral( "导入失败，原工作区已从恢复点还原。" ) );
    return false;
  }

  QFile::remove( pending_path );
  if( restore_folder )
    *restore_folder = restore_path;
  return true;
}
