#include <QtCore>

#include "ShadowWorkspaceArchive.h"


static bool writeFile( const QString& path, const QByteArray& data )
{
  QFile file( path );
  return file.open( QIODevice::WriteOnly ) && file.write( data ) == data.size();
}

static QByteArray readFile( const QString& path )
{
  QFile file( path );
  return file.open( QIODevice::ReadOnly ) ? file.readAll() : QByteArray();
}

int main( int argc, char** argv )
{
  QCoreApplication app( argc, argv );
  QTemporaryDir temporary_root;
  if( !temporary_root.isValid() )
    return 1;

  const QString source_path = QDir( temporary_root.path() ).filePath( QStringLiteral( "source" ) );
  const QString target_path = QDir( temporary_root.path() ).filePath( QStringLiteral( "target" ) );
  QDir().mkpath( source_path );
  QDir().mkpath( target_path );
  const QByteArray preferences("[peer-fixture]\npinned=true\nmuted=true\nhidden=false\n");
  if( !writeFile( source_path + QStringLiteral( "/conversation-preferences.ini" ), preferences ) ) return 23;
  const QByteArray peerAvatars("[peer-fixture]\nphoto=fixture-only\n");
  if( !writeFile( source_path + QStringLiteral( "/peer-avatars.ini" ), peerAvatars ) ) return 25;
  if( !writeFile( QDir( source_path ).filePath( QStringLiteral( "beebeep.ini" ) ), QByteArrayLiteral( "profile=new" ) ) ||
      !writeFile( QDir( source_path ).filePath( QStringLiteral( "beebeep.dat" ) ), QByteArrayLiteral( "chat=new" ) ) ||
      !writeFile( QDir( target_path ).filePath( QStringLiteral( "beebeep.ini" ) ), QByteArrayLiteral( "profile=old" ) ) )
    return 2;

  const QString archive_path = QDir( temporary_root.path() ).filePath( QStringLiteral( "roundtrip.beebeep-workspace" ) );
  QString error_message;
  if( !ShadowWorkspaceArchive::exportWorkspace( source_path, archive_path, &error_message ) )
  {
    qCritical() << error_message;
    return 3;
  }
  if( !ShadowWorkspaceArchive::stageImport( target_path, archive_path, &error_message ) )
  {
    qCritical() << error_message;
    return 4;
  }

  QString restore_folder;
  if( !ShadowWorkspaceArchive::applyPendingImport( target_path, &restore_folder, &error_message ) )
  {
    qCritical() << error_message;
    return 5;
  }
  if( !readFile( QDir( target_path ).filePath( QStringLiteral( "beebeep.ini" ) ) ).contains( QByteArrayLiteral( "profile=new" ) ) ||
      readFile( QDir( target_path ).filePath( QStringLiteral( "beebeep.dat" ) ) ) != QByteArrayLiteral( "chat=new" ) )
    return 6;
  if( restore_folder.isEmpty() || readFile( QDir( restore_folder ).filePath( QStringLiteral( "beebeep.ini" ) ) ) != QByteArrayLiteral( "profile=old" ) )
    return 7;
  if( QFileInfo::exists( ShadowWorkspaceArchive::pendingFilePath( target_path ) ) )
    return 8;
  if( readFile( target_path + QStringLiteral( "/conversation-preferences.ini" ) ) != preferences ) return 24;
  if( readFile( target_path + QStringLiteral( "/peer-avatars.ini" ) ) != peerAvatars ) return 26;

  QFile archive( archive_path );
  if( !archive.open( QIODevice::ReadWrite ) || !archive.seek( qMax<qint64>( 0, archive.size() - 1 ) ) )
    return 9;
  const QByteArray last_byte_data = archive.read( 1 );
  if( last_byte_data.size() != 1 )
    return 9;
  const char last_byte = last_byte_data.at( 0 );
  archive.seek( archive.size() - 1 );
  const char changed_byte = static_cast<char>( last_byte ^ 0x7f );
  archive.write( &changed_byte, 1 );
  archive.close();

  if( ShadowWorkspaceArchive::stageImport( target_path, archive_path, &error_message ) )
    return 10;

  qInfo() << "workspace archive roundtrip and corruption checks passed";
  const QString media = QDir( source_path ).filePath( QStringLiteral( "received" ) );
  QDir().mkpath( media + QStringLiteral( "/project/nested" ) );
  if( !writeFile( media + QStringLiteral( "/project/nested/texture.png" ), QByteArray( 2 * 1024 * 1024 + 19, 'x' ) ) ) return 11;
  if( !writeFile( source_path + QStringLiteral( "/.shadow-account.ini" ), QByteArrayLiteral( "email=alice@example.com\npassword=source\n" ) ) ) return 12;
  if( !writeFile( target_path + QStringLiteral( "/.shadow-account.ini" ), QByteArrayLiteral( "email=alice@example.com\npassword=target\n" ) ) ) return 13;
  const QString bundle = temporary_root.path() + QStringLiteral( "/attachments.beebeep-workspace" );
  if( !ShadowWorkspaceArchive::exportWithAttachments( source_path, media, bundle, &error_message ) ) { qCritical()<<error_message;return 14; }
  QString email;
  if( !ShadowWorkspaceArchive::archiveEmail( bundle, &email, &error_message ) || email != QStringLiteral( "alice@example.com" ) ) return 15;
  if( !ShadowWorkspaceArchive::stageImport( target_path, bundle, &error_message ) || !ShadowWorkspaceArchive::applyPendingImport( target_path, &restore_folder, &error_message ) ) {qCritical()<<error_message;return 16;}
  QSettings migration( target_path + QStringLiteral( "/.shadow-paths.ini" ), QSettings::IniFormat );
  if( readFile( migration.value( "targetRoot" ).toString() + QStringLiteral( "/project/nested/texture.png" ) ) != QByteArray( 2 * 1024 * 1024 + 19, 'x' ) ) return 17;
  if( !readFile( target_path + QStringLiteral( "/.shadow-account.ini" ) ).contains( "password=target" ) ) return 18;
  if( !writeFile( target_path + QStringLiteral( "/.shadow-account.ini" ), QByteArrayLiteral( "email=bob@example.com\npassword=target\n" ) ) ) return 19;
  if( ShadowWorkspaceArchive::stageImport( target_path, bundle, &error_message ) ) return 20;
  QFile damaged( bundle ); if( !damaged.open( QIODevice::ReadWrite ) || !damaged.seek( damaged.size()-1 ) )return 21;
  const char changed = char(damaged.read(1).at(0)^0x7f); damaged.seek(damaged.size()-1); damaged.write(&changed,1);damaged.close();
  if( ShadowWorkspaceArchive::stageImport( target_path, bundle, &error_message ) )return 22;
  qInfo()<<"streaming attachment migration, credential preservation, account mismatch and corruption passed";
  return 0;
}
