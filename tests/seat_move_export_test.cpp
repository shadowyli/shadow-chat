#include "ShadowWorkspaceArchive.h"
#include <QtGui/private/qzipreader_p.h>
#include <cstdio>

static bool writeFile( const QString& path, const QByteArray& bytes )
{
  QFile file( path );
  return file.open( QIODevice::WriteOnly ) && file.write( bytes ) == bytes.size();
}

static QByteArray readFile( const QString& path )
{
  QFile file( path );
  return file.open( QIODevice::ReadOnly ) ? file.readAll() : QByteArray();
}

int main( int argc, char** argv )
{
  QCoreApplication app( argc, argv );
  QTemporaryDir temporary;
  const QString account = "tester+seat@example.test";
  const QDateTime timestamp( QDate( 2026, 11, 5 ), QTime( 10, 30, 12 ) );
  int failures = 0;
  auto check = [&]( bool value, const char* name ) {
    std::printf( "%s %s\n", value ? "PASS" : "FAIL", name );
    if( !value ) ++failures;
  };
  check( ShadowWorkspaceArchive::seatMoveFileName( " TESTER+SEAT@EXAMPLE.TEST ", timestamp ) ==
    QStringLiteral( "影聊_换座位历史记录_tester+seat@example.test_20261105_103012.zip" ),
    "filename includes product, purpose, normalized account and date/time" );
  check( ShadowWorkspaceArchive::seatMoveFileName( "../outside", timestamp ).isEmpty() &&
    ShadowWorkspaceArchive::seatMoveFileName( account, QDateTime() ).isEmpty(),
    "unsafe account and invalid date rejected" );
  const QString profile = temporary.filePath( "profile" );
  check( QDir().mkpath( profile ), "temporary synthetic profile created" );
  const QString metadata = QDir( profile ).filePath( ".shadow-account.ini" );
  const QString history = QDir( profile ).filePath( "beebeep.dat" );
  const QString session = temporary.filePath( "session.ini" );
  const QByteArray credentials = "[General]\nemail=tester+seat@example.test\nsalt=synthetic-only\npassword=synthetic-hash-only\n";
  const QByteArray messages = "synthetic saved history bytes";
  const QByteArray remembered = "[General]\nautoLoginEnabled=true\nautoLoginEmail=tester+seat@example.test\n";
  check( writeFile( metadata, credentials ) && writeFile( history, messages ) &&
    writeFile( session, remembered ), "synthetic account, history and remembered session seeded" );
  const QString archive = temporary.filePath( ShadowWorkspaceArchive::seatMoveFileName( account, timestamp ) );
  QString error;
  check( ShadowWorkspaceArchive::exportSeatMoveZip( profile, archive, &error ), "actual ZIP export succeeds" );
  QZipReader reader( archive );
  check( reader.status() == QZipReader::NoError && reader.fileInfoList().size() == 3,
    "standard ZIP has exactly workspace, manifest and instructions" );
  const QJsonObject manifest = QJsonDocument::fromJson( reader.fileData( "manifest.json" ) ).object();
  check( manifest.value( "account" ).toString() == account &&
    manifest.value( "purpose" ).toString() == "seat-move-history" &&
    !manifest.value( "encrypted" ).toBool() && !manifest.value( "attachmentsIncluded" ).toBool(),
    "manifest declares account, purpose, encryption and attachment scope" );
  const QString restored = temporary.filePath( "restored.beebeep-workspace" );
  check( writeFile( restored, reader.fileData( "workspace.beebeep-workspace" ) ), "workspace entry extracted" );
  QString restoredEmail;
  check( ShadowWorkspaceArchive::archiveEmail( restored, &restoredEmail, &error ) && restoredEmail == account,
    "existing workspace reader validates exported restore payload" );
  check( reader.fileData( "README.txt" ).contains( account.toUtf8() ), "instructions include account identity" );
  reader.close();
  const QByteArray originalArchive = readFile( archive );
  check( !ShadowWorkspaceArchive::exportSeatMoveZip( profile, archive, &error ) &&
    readFile( archive ) == originalArchive, "existing backup never overwritten" );
  check( !ShadowWorkspaceArchive::exportSeatMoveZip( profile, QDir( profile ).filePath( "unsafe.zip" ), &error ) &&
    !QFileInfo::exists( QDir( profile ).filePath( "unsafe.zip" ) ), "backup inside account directory rejected" );
  check( !ShadowWorkspaceArchive::exportSeatMoveZip( profile, temporary.filePath( "missing/failure.zip" ), &error ),
    "missing target directory fails without state changes" );
  check( !ShadowWorkspaceArchive::exportSeatMoveZip( profile, temporary.filePath( "new-session.ini" ), &error ) &&
    !QFileInfo::exists( temporary.filePath( "new-session.ini" ) ), "non-ZIP target rejected" );
  check( readFile( metadata ) == credentials && readFile( history ) == messages && readFile( session ) == remembered,
    "export success and failure never logout, clear credentials or alter original records" );
  return failures ? 1 : 0;
}
