#include "ShadowWorkspaceArchive.h"
#include "ShadowLocalAccounts.h"
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
  const QString root = temporary.filePath( "accounts" );
  ShadowLocalAccounts accounts( root );
  check( accounts.create( account, "synthetic-password", &error ) &&
    accounts.create( "other@example.test", "other-password", &error ), "two isolated accounts created" );
  const QString accountProfile = accounts.profileFolder( account );
  const QString accountMetadata = QDir( accountProfile ).filePath( ".shadow-account.ini" );
  const QString accountHistory = QDir( accountProfile ).filePath( "beebeep.dat" );
  const QString otherMetadata = QDir( accounts.profileFolder( "other@example.test" ) ).filePath( ".shadow-account.ini" );
  const QByteArray otherCredentials = readFile( otherMetadata );
  check( writeFile( accountHistory, messages ), "real account history seeded" );
  accounts.rememberLogin( account );
  const QString sessionPath = QDir( root ).filePath( "session.ini" );
  const QByteArray sessionBefore = readFile( sessionPath );
  const QByteArray credentialsBefore = readFile( accountMetadata );
  check( !accounts.clearPasswordForSeatMove( "../outside", &error ) &&
    readFile( sessionPath ) == sessionBefore && readFile( accountMetadata ) == credentialsBefore,
    "invalid identity cannot change login or password" );
  const QString movedArchive = temporary.filePath( "real-seat-move.zip" );
  check( ShadowWorkspaceArchive::exportSeatMoveZip( accountProfile, movedArchive, &error ) &&
    accounts.authenticate( account, "synthetic-password" ) && accounts.autoLoginEmail() == account,
    "password remains usable until backup is committed" );
  check( accounts.clearPasswordForSeatMove( account, &error ), "seat move clears credentials after successful backup" );
  QSettings cleared( accountMetadata, QSettings::IniFormat );
  check( !cleared.contains( "password" ) && !cleared.contains( "salt" ) &&
    cleared.value( "email" ).toString() == account && accounts.exists( account ),
    "password and salt removed but account identity remains" );
  check( !accounts.autoLoginEnabled() && accounts.autoLoginEmail().isEmpty() &&
    !accounts.authenticate( account, "synthetic-password" ),
    "old password and automatic login are both disabled" );
  check( !accounts.create( account, "replacement-password", &error ) &&
    readFile( accountHistory ) == messages && readFile( otherMetadata ) == otherCredentials,
    "anonymous recreation cannot unlock old history and other account is unchanged" );
  accounts.rememberLogin( account );
  accounts.setAutoLoginEnabled( true );
  check( accounts.autoLoginEmail().isEmpty(), "stale or manually restored session cannot bypass cleared credentials" );
  QZipReader movedReader( movedArchive );
  const QString movedCore = temporary.filePath( "moved.beebeep-workspace" );
  check( writeFile( movedCore, movedReader.fileData( "workspace.beebeep-workspace" ) ),
    "backup can still be extracted after local password cleanup" );
  movedReader.close();
  ShadowLocalAccounts migrated( temporary.filePath( "new-computer" ) );
  const QString migratedProfile = migrated.profileFolder( account );
  QString restorePoint;
  check( ShadowWorkspaceArchive::stageImport( migratedProfile, movedCore, &error ) &&
    ShadowWorkspaceArchive::applyPendingImport( migratedProfile, &restorePoint, &error ) &&
    migrated.setPassword( account, "new-computer-password", &error ) &&
    migrated.authenticate( account, "new-computer-password" ) &&
    readFile( QDir( migratedProfile ).filePath( "beebeep.dat" ) ) == messages,
    "new computer restores original history and chooses a new password" );
  check( accounts.setRecoveryPassword( "synthetic-recovery" ) &&
    accounts.verifyRecoveryPassword( "synthetic-recovery" ) &&
    accounts.setPassword( account, "recovered-password", &error ) &&
    accounts.authenticate( account, "recovered-password" ),
    "authorized local password recovery remains available" );
  accounts.logout();
  QFile sessionFile( sessionPath );
  check( sessionFile.remove() && QDir().mkdir( sessionPath ), "invalid login target fixture created" );
  const QByteArray recoveryCredentials = readFile( accountMetadata );
  check( !accounts.clearPasswordForSeatMove( account, &error ) &&
    readFile( accountMetadata ) == recoveryCredentials,
    "login cleanup preflight failure never clears the password" );
  return failures ? 1 : 0;
}
