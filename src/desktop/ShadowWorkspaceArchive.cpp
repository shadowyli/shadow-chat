//////////////////////////////////////////////////////////////////////
//
// 影聊 portable workspace archive.
//
//////////////////////////////////////////////////////////////////////

#include "ShadowWorkspaceArchive.h"
#include "ShadowAttachmentBundle.h"

#include <QCryptographicHash>
#include <QSaveFile>
#include <QBuffer>
#include <QLockFile>
#include <QJsonDocument>
#include <QJsonObject>


namespace
{
  const quint32 ShadowWorkspaceMagic = 0x53435753; // SCWS
  const quint16 ShadowWorkspaceVersion = 1;
  const quint32 ShadowWorkspaceMaxEntries = 16;
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
                       << QStringLiteral( "beehosts.ini" )
                       << QStringLiteral( ".shadow-account.ini" )
                       << QStringLiteral( ".shadow-paths.ini" )
                       << QStringLiteral( "pending-transfers.json" )
                       << QStringLiteral( "message-delivery.json" )
                       << QStringLiteral( "conversation-preferences.ini" )
                       << QStringLiteral( "peer-avatars.ini" );
}

qint64 ShadowWorkspaceArchive::attachmentSize( const QString& folder )
{
  return ShadowAttachmentBundle::estimate( folder );
}

bool ShadowWorkspaceArchive::exportWithAttachments( const QString& data_folder, const QString& attachment_folder, const QString& archive_path, QString* error_message )
{
  QTemporaryDir temporary;
  if( !temporary.isValid() ) { setError( error_message, QStringLiteral( "无法创建备份临时目录。" ) ); return false; }
  const QString core = QDir( temporary.path() ).filePath( QStringLiteral( "core.workspace" ) );
  return exportWorkspace( data_folder, core, error_message ) &&
         ShadowAttachmentBundle::write( core, attachment_folder, archive_path, error_message );
}

QString ShadowWorkspaceArchive::seatMoveFileName( const QString& account, const QDateTime& timestamp )
{
  const QString normalized = account.trimmed().toLower();
  const QRegularExpression address( QStringLiteral( "^[a-z0-9][a-z0-9._+-]*@[a-z0-9][a-z0-9.-]*\\.[a-z]{2,}$" ) );
  if( normalized.size() > 180 || !address.match( normalized ).hasMatch() || !timestamp.isValid() )
    return QString();
  return QStringLiteral( "影聊_换座位历史记录_%1_%2.zip" )
    .arg( normalized, timestamp.toString( QStringLiteral( "yyyyMMdd_HHmmss" ) ) );
}

bool ShadowWorkspaceArchive::exportSeatMoveZip( const QString& data_folder, const QString& archive_path, QString* error_message )
{
  if( error_message ) error_message->clear();
  auto fail = [&]( const QString& message ) { setError( error_message, message ); return false; };
  const QString profile = QFileInfo( data_folder ).canonicalFilePath();
  const QFileInfo target( archive_path );
  if( target.suffix().compare( QStringLiteral( "zip" ), Qt::CaseInsensitive ) != 0 )
    return fail( QStringLiteral( "换座位备份需使用 .zip 文件名。" ) );
  const QString parent = QFileInfo( target.absolutePath() ).canonicalFilePath();
  const Qt::CaseSensitivity sensitivity =
#ifdef Q_OS_WIN
    Qt::CaseInsensitive;
#else
    Qt::CaseSensitive;
#endif
  if( profile.isEmpty() || parent.isEmpty() || archive_path.isEmpty() ||
      parent.compare( profile, sensitivity ) == 0 ||
      parent.startsWith( profile + QLatin1Char( '/' ), sensitivity ) )
    return fail( QStringLiteral( "请将换座位备份保存到账号目录之外的已有文件夹。" ) );
  QLockFile lock( target.absoluteFilePath() + QStringLiteral( ".seat-export.lock" ) );
  if( !lock.tryLock() ) return fail( QStringLiteral( "这个备份位置正在写入，请选择其他名称。" ) );
  if( target.exists() || target.isSymLink() )
    return fail( QStringLiteral( "备份文件已存在，不会覆盖，请选择其他名称。" ) );
  const QString metadataPath = QDir( profile ).filePath( QStringLiteral( ".shadow-account.ini" ) );
  if( !QFileInfo::exists( metadataPath ) ) return fail( QStringLiteral( "无法识别当前账号，尚未导出。" ) );
  QSettings account( metadataPath, QSettings::IniFormat );
  const QString email = account.value( "email" ).toString().trimmed().toLower();
  const QDateTime now = QDateTime::currentDateTime();
  if( seatMoveFileName( email, now ).isEmpty() )
    return fail( QStringLiteral( "当前账号名无效，尚未导出。" ) );
  QTemporaryDir temporary;
  if( !temporary.isValid() ) return fail( QStringLiteral( "无法创建备份临时目录。" ) );
  const QString corePath = temporary.filePath( QStringLiteral( "workspace.beebeep-workspace" ) );
  if( !exportWorkspace( profile, corePath, error_message ) ) return false;
  QString archivedEmail;
  if( !archiveEmail( corePath, &archivedEmail, error_message ) || archivedEmail != email )
    return fail( QStringLiteral( "备份账号校验失败，原账号与登录状态未改变。" ) );
  struct ZipInput
  {
    QByteArray name;
    QString path;
    QByteArray bytes;
    quint32 crc = 0, size = 0, offset = 0;
  };
  QJsonObject metadata;
  metadata.insert( "formatVersion", 1 );
  metadata.insert( "purpose", "seat-move-history" );
  metadata.insert( "account", email );
  metadata.insert( "createdAt", now.toString( Qt::ISODate ) );
  metadata.insert( "workspace", "workspace.beebeep-workspace" );
  metadata.insert( "attachmentsIncluded", false );
  metadata.insert( "encrypted", false );
  QList<ZipInput> inputs;
  ZipInput workspace;
  workspace.name = "workspace.beebeep-workspace"; workspace.path = corePath; inputs.append( workspace );
  ZipInput manifest;
  manifest.name = "manifest.json"; manifest.bytes = QJsonDocument( metadata ).toJson(); inputs.append( manifest );
  ZipInput instructions;
  instructions.name = "README.txt";
  instructions.bytes = QStringLiteral(
    "影聊换座位历史备份\n账号：%1\n\n"
    "解压后，在新电脑影聊登录页填写此账号及新密码，选择恢复工作区，导入 workspace.beebeep-workspace。\n"
    "不要先创建同名账号；若已创建，请登录后使用“工作区与换机 → 导入工作区”。\n"
    "包含账号验证资料、聊天记录和设置，不包含收到的图片/文件附件；没有包含明文密码。\n"
    "备份未加密，请妥善保管。导出本身不退出账号、不修改密码、不删除原记录。\n" ).arg( email ).toUtf8();
  inputs.append( instructions );
  auto updateCrc = []( quint32 checksum, const QByteArray& bytes ) {
    quint32 table[256];
    for( quint32 index = 0; index < 256; ++index )
    {
      quint32 value = index;
      for( int bit = 0; bit < 8; ++bit ) value = (value >> 1) ^ ((value & 1) ? 0xedb88320U : 0U);
      table[index] = value;
    }
    for( unsigned char byte : bytes ) checksum = table[(checksum ^ byte) & 255] ^ (checksum >> 8);
    return checksum;
  };
  QSaveFile output( target.absoluteFilePath() );
  output.setDirectWriteFallback( false );
  if( !output.open( QIODevice::WriteOnly ) ) return fail( QStringLiteral( "无法创建备份文件，请检查保存权限。" ) );
  QDataStream stream( &output );
  stream.setByteOrder( QDataStream::LittleEndian );
  const quint16 time = quint16( (now.time().hour() << 11) | (now.time().minute() << 5) | (now.time().second()/2) );
  const quint16 date = quint16( ((qBound( 1980, now.date().year(), 2107 )-1980) << 9) |
    (now.date().month() << 5) | now.date().day() );
  for( ZipInput& entry : inputs )
  {
    QFile file( entry.path );
    QBuffer buffer( &entry.bytes );
    QIODevice* input = entry.path.isEmpty() ? static_cast<QIODevice*>( &buffer ) : static_cast<QIODevice*>( &file );
    if( !input->open( QIODevice::ReadOnly ) || input->size() < 0 ||
        quint64( input->size() ) > ShadowWorkspaceMaxArchiveSize )
      return fail( QStringLiteral( "备份条目无法读取或超过大小限制。" ) );
    entry.size = quint32( input->size() );
    quint32 crc = 0xffffffffU;
    qint64 read = 0;
    while( !input->atEnd() )
    {
      const QByteArray bytes = input->read( 1024*1024 );
      if( bytes.isEmpty() ) return fail( QStringLiteral( "读取备份失败，原记录未改变。" ) );
      read += bytes.size(); crc = updateCrc( crc, bytes );
    }
    if( read != entry.size || !input->seek( 0 ) ) return fail( QStringLiteral( "备份读取不完整。" ) );
    entry.crc = crc ^ 0xffffffffU;
    entry.offset = quint32( output.pos() );
    stream << quint32( 0x04034b50 ) << quint16( 20 ) << quint16( 0x0800 ) << quint16( 0 )
           << time << date << entry.crc << entry.size << entry.size << quint16( entry.name.size() ) << quint16( 0 );
    if( output.write( entry.name ) != entry.name.size() ) return fail( QStringLiteral( "ZIP写入失败。" ) );
    quint32 copiedCrc = 0xffffffffU;
    qint64 copied = 0;
    while( !input->atEnd() )
    {
      const QByteArray bytes = input->read( 1024*1024 );
      if( bytes.isEmpty() || output.write( bytes ) != bytes.size() ) return fail( QStringLiteral( "ZIP写入失败，请检查磁盘空间。" ) );
      copied += bytes.size(); copiedCrc = updateCrc( copiedCrc, bytes );
    }
    if( copied != entry.size || (copiedCrc ^ 0xffffffffU) != entry.crc )
      return fail( QStringLiteral( "备份内容在写入中改变，请重试。" ) );
  }
  const quint32 directoryOffset = quint32( output.pos() );
  for( const ZipInput& entry : inputs )
  {
    stream << quint32( 0x02014b50 ) << quint16( 20 ) << quint16( 20 ) << quint16( 0x0800 )
           << quint16( 0 ) << time << date << entry.crc << entry.size << entry.size
           << quint16( entry.name.size() ) << quint16( 0 ) << quint16( 0 ) << quint16( 0 )
           << quint16( 0 ) << quint32( 0 ) << entry.offset;
    if( output.write( entry.name ) != entry.name.size() ) return fail( QStringLiteral( "ZIP目录写入失败。" ) );
  }
  const quint32 directorySize = quint32( output.pos() ) - directoryOffset;
  stream << quint32( 0x06054b50 ) << quint16( 0 ) << quint16( 0 ) << quint16( inputs.size() )
         << quint16( inputs.size() ) << directorySize << directoryOffset << quint16( 0 );
  if( stream.status() != QDataStream::Ok || !output.commit() )
    return fail( QStringLiteral( "备份提交失败，原记录与登录状态未改变。" ) );
  return true;
}

bool ShadowWorkspaceArchive::exportWorkspace( const QString& data_folder, const QString& archive_path, QString* error_message )
{
  QList<Entry> entries;
  quint64 total_size = 0;
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
    if( static_cast<quint64>( file.size() ) > ShadowWorkspaceMaxEntrySize ||
        total_size + static_cast<quint64>( file.size() ) > ShadowWorkspaceMaxTotalSize )
    {
      setError( error_message, QStringLiteral( "%1 过大，无法加入工作区。" ).arg( file_name ) );
      return false;
    }
    Entry entry;
    entry.name = file_name;
    entry.data = file.readAll();
    if( entry.data.size() != file.size() )
    {
      setError( error_message, QStringLiteral( "读取 %1 失败。" ).arg( file_name ) );
      return false;
    }
    total_size += static_cast<quint64>( entry.data.size() );
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

  if( ShadowAttachmentBundle::isBundle( archive_path ) )
  {
    QTemporaryDir temporary;
    const QString core = QDir( temporary.path() ).filePath( QStringLiteral( "core.workspace" ) );
    return temporary.isValid() && ShadowAttachmentBundle::unpack( archive_path, core, QString(), Q_NULLPTR, error_message ) &&
           readArchive( core, entries, error_message );
  }

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

bool ShadowWorkspaceArchive::archiveEmail( const QString& archive_path, QString* email, QString* error_message )
{
  QList<Entry> entries;
  if( !readArchive( archive_path, &entries, error_message ) ) return false;
  if( email ) email->clear();
  foreach( const Entry& entry, entries )
  {
    if( entry.name != QStringLiteral( ".shadow-account.ini" ) ) continue;
    QTemporaryFile temporary;
    if( !temporary.open() || temporary.write( entry.data ) != entry.data.size() || !temporary.flush() )
    {
      setError( error_message, QStringLiteral( "无法读取备份账号资料。" ) );
      return false;
    }
    QSettings metadata( temporary.fileName(), QSettings::IniFormat );
    if( email ) *email = metadata.value( QStringLiteral( "email" ) ).toString().trimmed().toLower();
  }
  return true;
}

bool ShadowWorkspaceArchive::stageImport( const QString& data_folder, const QString& archive_path, QString* error_message )
{
  QList<Entry> entries;
  if( !readArchive( archive_path, &entries, error_message ) )
    return false;
  const QString credentials_path = QDir( data_folder ).filePath( QStringLiteral( ".shadow-account.ini" ) );
  if( QFileInfo::exists( credentials_path ) )
  {
    QSettings credentials( credentials_path, QSettings::IniFormat );
    QString incoming_email;
    if( !archiveEmail( archive_path, &incoming_email, error_message ) ) return false;
    if( !incoming_email.isEmpty() && incoming_email != credentials.value( QStringLiteral( "email" ) ).toString().trimmed().toLower() )
    {
      setError( error_message, QStringLiteral( "备份属于其他邮箱账号，请退出后从登录页恢复。" ) );
      return false;
    }
  }

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
  if( !pending.open( QIODevice::WriteOnly ) )
  {
    setError( error_message, QStringLiteral( "无法暂存工作区，请检查数据目录权限。" ) );
    return false;
  }
  while( !source.atEnd() )
  {
    const QByteArray chunk = source.read( 1024 * 1024 );
    if( chunk.isEmpty() || pending.write( chunk ) != chunk.size() )
    {
      setError( error_message, QStringLiteral( "备份暂存失败，请检查剩余空间。" ) );
      return false;
    }
  }
  if( !pending.commit() ) { setError( error_message, QStringLiteral( "无法保存待恢复备份。" ) ); return false; }
  return true;
}

bool ShadowWorkspaceArchive::applyPendingImport( const QString& data_folder, QString* restore_folder, QString* error_message )
{
  const QString pending_path = pendingFilePath( data_folder );
  if( !QFileInfo::exists( pending_path ) )
    return true;

  if( ShadowAttachmentBundle::isBundle( pending_path ) )
  {
    QTemporaryDir temporary( QDir( data_folder ).filePath( QStringLiteral( ".restore-XXXXXX" ) ) );
    if( !temporary.isValid() ) { setError( error_message, QStringLiteral( "无法创建恢复临时目录。" ) ); return false; }
    const QString core = QDir( temporary.path() ).filePath( QStringLiteral( "core.workspace" ) );
    const QString attachments = QDir( temporary.path() ).filePath( QStringLiteral( "attachments" ) );
    QString original_root;
    if( !ShadowAttachmentBundle::unpack( pending_path, core, attachments, &original_root, error_message ) ) return false;
    // Restore into a fresh directory; existing received files are never replaced.
    const QString destination = QDir( data_folder ).filePath( QString::fromUtf8( "接收文件/迁移-%1" ).arg( QUuid::createUuid().toString( QUuid::WithoutBraces ) ) );
    QDir().mkpath( QFileInfo( destination ).absolutePath() );
    if( !QDir().mkpath( attachments ) || !QDir().rename( attachments, destination ) )
    { setError( error_message, QStringLiteral( "无法保存恢复的附件，原记录未改变。" ) ); return false; }
    const QString envelope = QDir( temporary.path() ).filePath( QStringLiteral( "envelope.workspace" ) );
    if( !QFile::rename( pending_path, envelope ) ) { QDir( destination ).removeRecursively(); return false; }
    QString restored;
    if( !stageImport( data_folder, core, error_message ) || !applyPendingImport( data_folder, &restored, error_message ) )
    {
      QFile::remove( pending_path ); QFile::rename( envelope, pending_path );
      QDir( destination ).removeRecursively();
      return false;
    }
    QSettings mapping( QDir( data_folder ).filePath( QStringLiteral( ".shadow-paths.ini" ) ), QSettings::IniFormat );
    mapping.setValue( QStringLiteral( "sourceRoot" ), original_root ); mapping.setValue( QStringLiteral( "targetRoot" ), destination ); mapping.sync();
    QSettings profile( QDir( data_folder ).filePath( QStringLiteral( "beebeep.ini" ) ), QSettings::IniFormat );
    profile.setValue( QStringLiteral( "Gui/DownloadDirectory" ), destination );
    profile.setValue( QStringLiteral( "Gui/LastDirectorySelected" ), destination ); profile.sync();
    if( restore_folder ) *restore_folder = restored;
    return true;
  }

  QList<Entry> entries;
  if( !readArchive( pending_path, &entries, error_message ) )
    return false;

  QDir data_dir( data_folder );
  // Existing account imports preserve the destination login credentials.
  QFile credentials( data_dir.filePath( QStringLiteral( ".shadow-account.ini" ) ) );
  if( credentials.exists() )
  {
    if( !credentials.open( QIODevice::ReadOnly ) )
    {
      setError( error_message, QStringLiteral( "无法读取当前账号资料。" ) );
      return false;
    }
    const QByteArray existing_credentials = credentials.readAll();
    credentials.close();
    bool found = false;
    for( int i = 0; i < entries.size(); ++i )
      if( entries[i].name == QStringLiteral( ".shadow-account.ini" ) ) { entries[i].data = existing_credentials; found = true; }
    if( !found ) { Entry entry; entry.name = QStringLiteral( ".shadow-account.ini" ); entry.data = existing_credentials; entries.append( entry ); }
  }
  const QString restore_name = QStringLiteral( "workspace-restore-%1-%2" ).arg( QDateTime::currentDateTime().toString( QStringLiteral( "yyyyMMdd-HHmmss" ) ), QUuid::createUuid().toString( QUuid::WithoutBraces ) );
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
    bool rollback_ok = true;
    foreach( const QString& name, allowed_names )
      if( QFileInfo::exists( data_dir.filePath( name ) ) && !QFile::remove( data_dir.filePath( name ) ) ) rollback_ok = false;
    foreach( const QString& name, originally_present )
      if( !QFile::copy( QDir( restore_path ).filePath( name ), data_dir.filePath( name ) ) ) rollback_ok = false;
    setError( error_message, rollback_ok ? QStringLiteral( "导入失败，原工作区已从恢复点还原。" )
      : QStringLiteral( "导入失败，自动还原未完成。原数据仍在恢复点：%1。请关闭影聊并保留此目录。" ).arg( restore_path ) );
    return false;
  }

  QSettings imported_settings( data_dir.filePath( QStringLiteral( "beebeep.ini" ) ), QSettings::IniFormat );
  const QString own_download_folder = data_dir.filePath( QString::fromUtf8( "接收文件" ) );
  QDir().mkpath( own_download_folder );
  imported_settings.setValue( QStringLiteral( "Gui/DownloadDirectory" ), own_download_folder );
  imported_settings.setValue( QStringLiteral( "Gui/LastDirectorySelected" ), own_download_folder );
  imported_settings.sync();
  QFile::remove( pending_path );
  if( restore_folder )
    *restore_folder = restore_path;
  return true;
}
