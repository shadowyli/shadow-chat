#include <QtCore>
#include "ShadowTransferQueue.h"

int main( int argc, char** argv )
{
  QCoreApplication application( argc, argv );
  QTemporaryDir temporary;
  if( !temporary.isValid() ) return 1;
  int failures = 0;
  auto check = [&]( bool success, const char* label ) { if( !success ) { qCritical() << label; ++failures; } };
  QDir root( temporary.path() );
  root.mkpath( "account-a" ); root.mkpath( "account-b" ); root.mkpath( "materials/sub" );
  root.mkpath( "materials/empty/child" );
  const QString filePath = root.filePath( "materials/sub/large.psd" );
  QFile file( filePath );
  check( file.open( QIODevice::WriteOnly ), "fixture open" );
  check( file.resize( Q_INT64_C(5368709120) ), "5GB metadata fixture" );
  file.close();
  QString error;
  ShadowTransferQueue queue;
  check( queue.open( root.filePath( "account-a" ) ), "open account queue" );
  check( queue.enqueue( "recipient-a", "group-a", true, root.filePath( "materials" ), &error ), "queue folder" );
  check( queue.enqueue( "recipient-b", "group-a", true, filePath, &error ), "queue second recipient" );
  check( queue.tasks().size() == 2, "per recipient queue" );
  check( queue.tasks().first().directories.size() == 3, "empty subdirectory structure retained" );
  check( ShadowTransferQueue::sourceUnchanged( queue.tasks().first(), &error ), "unchanged large source" );
  check( queue.tasks().first().files.first().toObject()["size"].toString() == "5368709120", "64 bit size survives JSON" );
  check( QDir( root.filePath( "account-a" ) ).entryList( QDir::Files ).size() == 1, "only metadata stored, no material copies" );
  ShadowTransferQueue::Task offered = queue.tasks().first();
  offered.state = "offered"; offered.completed << filePath;
  check( queue.update( offered ), "save active task" );
  ShadowTransferQueue restarted;
  check( restarted.open( root.filePath( "account-a" ) ), "restart load" );
  check( restarted.tasks().first().state == "pending" && restarted.tasks().first().completed.isEmpty(), "restart begins whole transfer again" );
  check( restarted.remove( offered.id ) && restarted.tasks().size() == 1 && restarted.tasks().first().recipient == "recipient-b", "cancel departed recipient only" );
  check( restarted.open( root.filePath( "account-b" ) ) && restarted.tasks().isEmpty(), "account queue isolation" );
  QFile added( root.filePath( "materials/added.txt" ) );
  added.open( QIODevice::WriteOnly ); added.write( "extra" ); added.close();
  check( !ShadowTransferQueue::sourceUnchanged( offered, &error ), "folder additions rejected" );
  added.remove();
  file.open( QIODevice::ReadWrite );
  file.setFileTime( QFileInfo( filePath ).lastModified().addSecs( 5 ), QFileDevice::FileModificationTime );
  file.close();
  check( !ShadowTransferQueue::sourceUnchanged( offered, &error ), "same size modified source rejected" );
  file.open( QIODevice::WriteOnly ); file.resize( 4 ); file.close();
  check( !ShadowTransferQueue::sourceUnchanged( offered, &error ), "modified source rejected" );
  QFile::remove( filePath );
  check( !ShadowTransferQueue::sourceUnchanged( offered, &error ), "deleted source rejected" );
  QFile corrupt( root.filePath( "account-b/pending-transfers.json" ) );
  corrupt.open( QIODevice::WriteOnly ); corrupt.write( "bad json" ); corrupt.close();
  ShadowTransferQueue invalid;
  check( !invalid.open( root.filePath( "account-b" ) ) && !invalid.open( root.filePath( "account-b" ) ), "corrupt queue never silently overwritten" );
  ShadowTransferQueue unwritable;
  check( unwritable.open( root.filePath( "missing/account" ) ), "open nonexistent metadata parent" );
  QFile fixture( root.filePath( "small.txt" ) ); fixture.open( QIODevice::WriteOnly ); fixture.write( "x" ); fixture.close();
  check( !unwritable.enqueue( "recipient", "", false, fixture.fileName(), &error ) && unwritable.tasks().isEmpty(), "failed save cannot report queued" );
  return failures ? 1 : 0;
}
