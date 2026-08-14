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
  if( readFile( QDir( target_path ).filePath( QStringLiteral( "beebeep.ini" ) ) ) != QByteArrayLiteral( "profile=new" ) ||
      readFile( QDir( target_path ).filePath( QStringLiteral( "beebeep.dat" ) ) ) != QByteArrayLiteral( "chat=new" ) )
    return 6;
  if( restore_folder.isEmpty() || readFile( QDir( restore_folder ).filePath( QStringLiteral( "beebeep.ini" ) ) ) != QByteArrayLiteral( "profile=old" ) )
    return 7;
  if( QFileInfo::exists( ShadowWorkspaceArchive::pendingFilePath( target_path ) ) )
    return 8;

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
  return 0;
}
