#include "BeeApplication.h"
#include "Core.h"
#include "GuiMain.h"
#include "Settings.h"
#include "Random.h"
#include "ShadowStyle.h"
#include <QtWidgets>
#include <QtTest>
#include <cstdio>
#include <windows.h>
#include <aclapi.h>

#define CHECK(value) do { if( !(value) ) { std::fprintf( stderr, "FAIL line %d: %s\n", __LINE__, #value ); return 1; } } while(false)

class RegistryFixture
{
public:
  RegistryFixture() : handle( nullptr ), active( false ), owner( false )
  {
    name = qEnvironmentVariable( "SHADOW_STARTUP_REGISTRY_FIXTURE" );
    owner = name.isEmpty();
    if( owner )
      name = QStringLiteral( "Software\\ShadowChatStartupTest-%1" ).arg( QUuid::createUuid().toString( QUuid::WithoutBraces ) );
    if( !name.startsWith( "Software\\ShadowChatStartupTest-" ) )
      return;
    if( RegCreateKeyExW( HKEY_CURRENT_USER, reinterpret_cast<LPCWSTR>( name.utf16() ), 0, nullptr, 0,
                        KEY_ALL_ACCESS, nullptr, &handle, nullptr ) == ERROR_SUCCESS )
      active = RegOverridePredefKey( HKEY_CURRENT_USER, handle ) == ERROR_SUCCESS;
  }
  ~RegistryFixture()
  {
    if( active ) RegOverridePredefKey( HKEY_CURRENT_USER, nullptr );
    if( handle ) RegCloseKey( handle );
    if( owner && active ) RegDeleteTreeW( HKEY_CURRENT_USER, reinterpret_cast<LPCWSTR>( name.utf16() ) );
  }
  bool valid() const { return active; }
  QString key() const { return name; }
private:
  QString name;
  HKEY handle;
  bool active;
  bool owner;
};

class RegistryWriteBlock
{
public:
  explicit RegistryWriteBlock( const QString& path ) : handle( nullptr ), descriptor( nullptr ), original( nullptr ), active( false ), protectedDacl( false )
  {
    if( RegOpenKeyExW( HKEY_CURRENT_USER, reinterpret_cast<LPCWSTR>( path.utf16() ), 0, KEY_ALL_ACCESS, &handle ) != ERROR_SUCCESS )
      return;
    if( GetSecurityInfo( handle, SE_REGISTRY_KEY, DACL_SECURITY_INFORMATION, nullptr, nullptr, &original, nullptr, &descriptor ) != ERROR_SUCCESS )
      return;
    SECURITY_DESCRIPTOR_CONTROL control;
    DWORD revision;
    if( !GetSecurityDescriptorControl( descriptor, &control, &revision ) )
      return;
    protectedDacl = (control & SE_DACL_PROTECTED) != 0;
    if( InitializeAcl( &denied, sizeof( denied ), ACL_REVISION ) )
      active = SetSecurityInfo( handle, SE_REGISTRY_KEY, DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
        nullptr, nullptr, &denied, nullptr ) == ERROR_SUCCESS;
  }
  ~RegistryWriteBlock()
  {
    if( active )
      SetSecurityInfo( handle, SE_REGISTRY_KEY, DACL_SECURITY_INFORMATION |
        (protectedDacl ? PROTECTED_DACL_SECURITY_INFORMATION : UNPROTECTED_DACL_SECURITY_INFORMATION),
        nullptr, nullptr, original, nullptr );
    if( descriptor ) LocalFree( descriptor );
    if( handle ) RegCloseKey( handle );
  }
  bool valid() const { return active; }
private:
  HKEY handle;
  PSECURITY_DESCRIPTOR descriptor;
  PACL original;
  ACL denied;
  bool active;
  bool protectedDacl;
};

static QMap<QString, QByteArray> fileHashes( const QString& root )
{
  QMap<QString, QByteArray> hashes;
  QDirIterator iterator( root, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories );
  while( iterator.hasNext() )
  {
    QFile file( iterator.next() );
    if( file.open( QIODevice::ReadOnly ) )
      hashes.insert( QDir( root ).relativeFilePath( file.fileName() ), QCryptographicHash::hash( file.readAll(), QCryptographicHash::Sha256 ) );
  }
  return hashes;
}

int main( int argc, char** argv )
{
  std::setvbuf( stdout, nullptr, _IONBF, 0 );
  RegistryFixture registry;
  CHECK( registry.valid() );
  std::printf( "ISOLATION disposable HKCU subtree: %s\n", qPrintable( registry.key() ) );
  QCoreApplication::setAttribute( Qt::AA_EnableHighDpiScaling );
  BeeApplication application( argc, argv );
  application.setFont( ShadowStyle::applicationFont() );
  QTemporaryDir temporary;
  CHECK( temporary.isValid() );
  Random::init();
  Settings& settings = Settings::instance();
  settings.setDefaultFolders();
  settings.createLocalUser( "startup-fixture" );
  settings.setAccountDataFolder( temporary.path() );
  settings.load();
  settings.setDownloadDirectory( temporary.path() + "/received" );
  const QString preferenceKey = QStringLiteral( "HKEY_CURRENT_USER\\Software\\shadowyli\\ShadowChat" );
  const QString runKey = QStringLiteral( "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run" );
  auto preference = [&]() { return QSettings( preferenceKey, QSettings::NativeFormat ).value( "StartupEnabled", -1 ).toInt(); };
  auto command = [&]() { return QSettings( runKey, QSettings::NativeFormat ).value( settings.programName() ).toString(); };
  auto guarded = [&]() {
    const QString before = command();
    const int priorPreference = preference();
    return !settings.addStartOnSystemBoot() && !settings.removeStartOnSystemBoot() &&
      !settings.configureStartOnSystemBoot( false ) && !settings.hasStartOnSystemBoot() &&
      !settings.startOnSystemBootPreference() && command() == before && preference() == priorPreference;
  };
  if( application.arguments().contains( "--verify-disabled" ) )
  {
    CHECK( settings.configureStartOnSystemBoot( false ) && settings.configureStartOnSystemBoot( true ) );
    CHECK( !settings.hasStartOnSystemBoot() && !settings.startOnSystemBootPreference() );
    CHECK( preference() == 0 && command().isEmpty() );
    std::puts( "PASS independent process restart preserves explicit opt-out" );
    return 0;
  }
  if( application.arguments().contains( "--verify-isolation" ) )
  {
    application.setProperty( "shadowDevPreview", false );
    application.setProperty( "shadowAccountRoot", QVariant() );
    CHECK( guarded() );
    std::puts( "PASS argument isolation survives cleared dynamic properties" );
    return 0;
  }
  auto child = [&]( const QStringList& arguments ) {
    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert( "SHADOW_STARTUP_REGISTRY_FIXTURE", registry.key() );
    process.setProcessEnvironment( environment );
    process.start( application.applicationFilePath(), arguments );
    const bool finished = process.waitForFinished( 30000 );
    std::fputs( process.readAllStandardOutput().constData(), stdout );
    if( !finished || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0 )
    {
      process.kill();
      process.waitForFinished( 3000 );
      std::fputs( process.readAllStandardError().constData(), stderr );
      return false;
    }
    return true;
  };
  {
    QSettings run( runKey, QSettings::NativeFormat );
    run.setValue( "UnrelatedFixture", "preserve-me" );
    run.sync();
  }
  CHECK( settings.configureStartOnSystemBoot( false ) );
  CHECK( settings.hasStartOnSystemBoot() && settings.startOnSystemBootPreference() && preference() == 1 );
  const QString quoted = QStringLiteral( "\"%1\"" ).arg( QDir::toNativeSeparators( application.applicationFilePath() ) );
  CHECK( command() == quoted );
  std::puts( "PASS first launch defaults enabled and matches actual quoted Run command" );
  QStringList acceptedPaths;
  acceptedPaths << application.applicationFilePath().toUpper() << quoted.toLower()
    << "  " + quoted + " \t" << QDir::toNativeSeparators( application.applicationFilePath() );
  for( const QString& path : acceptedPaths )
  {
    QSettings run( runKey, QSettings::NativeFormat );
    run.setValue( settings.programName(), path );
    run.sync();
    CHECK( settings.hasStartOnSystemBoot() );
  }
  QStringList rejectedPaths;
  rejectedPaths << quoted + " --invalid-flag" << quoted + " --minimized"
    << "\"" + application.applicationFilePath() << quoted + "\"" << quoted + " /S";
  for( const QString& path : rejectedPaths )
  {
    QSettings run( runKey, QSettings::NativeFormat );
    run.setValue( settings.programName(), path );
    run.sync();
    CHECK( !settings.hasStartOnSystemBoot() );
  }
  std::puts( "PASS case/separator/whitespace normalization; arguments and malformed quotes rejected" );
  CHECK( settings.addStartOnSystemBoot() );
  CHECK( settings.removeStartOnSystemBoot() );
  CHECK( !settings.hasStartOnSystemBoot() && !settings.startOnSystemBootPreference() && preference() == 0 && command().isEmpty() );
  CHECK( child( QStringList() << "--verify-disabled" ) );
  {
    QSettings prior( preferenceKey, QSettings::NativeFormat );
    prior.remove( "StartupEnabled" );
    prior.sync();
  }
  CHECK( settings.configureStartOnSystemBoot( true ) && command().isEmpty() && preference() == 0 );
  std::puts( "PASS legacy opt-out without explicit preference remains disabled" );
  CHECK( settings.addStartOnSystemBoot() );
  application.setProperty( "shadowAccountRoot", temporary.path() );
  CHECK( guarded() );
  application.setProperty( "shadowAccountRoot", QVariant() );
  application.setProperty( "shadowDevPreview", true );
  CHECK( guarded() );
  application.setProperty( "shadowDevPreview", false );
  CHECK( child( QStringList() << "--verify-isolation" << "--profile-root" << temporary.path() ) );
  CHECK( child( QStringList() << "--verify-isolation" << "--dev-preview" ) );
  std::puts( "PASS all guarded reads and writes preserve enabled registry state" );
  CHECK( settings.removeStartOnSystemBoot() );
  {
    RegistryWriteBlock blocked( QStringLiteral( "Software\\shadowyli\\ShadowChat" ) );
    CHECK( blocked.valid() );
    CHECK( !settings.addStartOnSystemBoot() && command().isEmpty() );
  }
  CHECK( preference() == 0 && command().isEmpty() );
  CHECK( settings.addStartOnSystemBoot() );
  {
    RegistryWriteBlock blocked( QStringLiteral( "Software\\shadowyli\\ShadowChat" ) );
    CHECK( blocked.valid() );
    CHECK( !settings.removeStartOnSystemBoot() && command() == quoted );
  }
  CHECK( preference() == 1 && command() == quoted );
  {
    RegistryWriteBlock blocked( QStringLiteral( "Software\\Microsoft\\Windows\\CurrentVersion\\Run" ) );
    CHECK( blocked.valid() );
    CHECK( !settings.addStartOnSystemBoot() && !settings.removeStartOnSystemBoot() );
  }
  CHECK( preference() == 1 && command() == quoted );
  std::puts( "PASS denied Run/preference writes return failure and restore the previous effective startup state" );
  Core core;
  GuiMain window;
  window.loadStyle();
  auto dialogAction = [&]( bool accept, bool expectedChecked, bool isolated, const QString& snapshot ) {
    bool inspected = false;
    bool valid = false;
    QTimer action;
    action.setSingleShot( true );
    QObject::connect( &action, &QTimer::timeout, [&]() {
      QDialog* dialog = qobject_cast<QDialog*>( QApplication::activeModalWidget() );
      if( !dialog ) return;
      inspected = true;
      QCheckBox* startup = dialog->findChild<QCheckBox*>( "ShadowStartupCheck" );
      valid = startup && startup->isChecked() == expectedChecked && startup->isEnabled() != isolated;
      if( startup )
      {
        valid = valid && startup->isVisible() && startup->width() >= startup->sizeHint().width() &&
          dialog->rect().contains( QRect( startup->mapTo( dialog, QPoint() ), startup->size() ) ) &&
          startup->visibleRegion().boundingRect().contains( startup->rect() );
        if( !isolated )
        {
          QTest::mouseClick( startup, Qt::LeftButton, Qt::NoModifier, QPoint( 8, startup->height() / 2 ) );
          valid = valid && startup->isChecked() != expectedChecked;
        }
      }
      if( !snapshot.isEmpty() ) valid = dialog->grab().save( snapshot ) && valid;
      QDialogButtonBox* buttons = dialog->findChild<QDialogButtonBox*>();
      QPushButton* button = buttons ? buttons->button( accept ? QDialogButtonBox::Save : QDialogButtonBox::Cancel ) : nullptr;
      if( button )
      {
        if( QScrollArea* scroll = dialog->findChild<QScrollArea*>() ) scroll->ensureWidgetVisible( button );
        valid = valid && button->isVisible() && button->visibleRegion().boundingRect().contains( button->rect() );
        QTest::mouseClick( button, Qt::LeftButton );
      }
      else
      {
        valid = false;
        dialog->reject();
      }
    } );
    QTimer timeout;
    timeout.setSingleShot( true );
    QObject::connect( &timeout, &QTimer::timeout, [&]() {
      if( QDialog* dialog = qobject_cast<QDialog*>( QApplication::activeModalWidget() ) ) dialog->reject();
    } );
    timeout.start( 6000 );
    action.start( 200 );
    const bool invoked = QMetaObject::invokeMethod( &window, "showShadowSettings", Qt::DirectConnection );
    return invoked && inspected && valid;
  };
  const auto beforeCancel = fileHashes( temporary.path() );
  const QString snapshots = qEnvironmentVariable( "SHADOW_STARTUP_SCREENSHOTS" );
  CHECK( dialogAction( false, true, false, snapshots.isEmpty() ? QString() : snapshots+"/settings-cancel.png" ) );
  CHECK( command() == quoted && preference() == 1 && fileHashes( temporary.path() ) == beforeCancel );
  std::puts( "PASS settings cancel writes neither startup preference nor account files" );
  CHECK( dialogAction( true, true, false, snapshots.isEmpty() ? QString() : snapshots+"/settings-disable.png" ) );
  CHECK( command().isEmpty() && preference() == 0 && !settings.startOnSystemBootPreference() );
  CHECK( child( QStringList() << "--verify-disabled" ) );
  CHECK( dialogAction( false, false, false, QString() ) );
  CHECK( command().isEmpty() && preference() == 0 );
  CHECK( settings.addStartOnSystemBoot() );
  application.setProperty( "shadowAccountRoot", temporary.path() );
  CHECK( dialogAction( true, false, true, snapshots.isEmpty() ? QString() : snapshots+"/settings-isolated.png" ) );
  application.setProperty( "shadowAccountRoot", QVariant() );
  CHECK( command() == quoted && preference() == 1 );
  application.setProperty( "shadowDevPreview", true );
  CHECK( dialogAction( true, false, true, QString() ) );
  application.setProperty( "shadowDevPreview", false );
  CHECK( command() == quoted && preference() == 1 );
  CHECK( QSettings( runKey, QSettings::NativeFormat ).value( "UnrelatedFixture" ) == "preserve-me" );
  if( application.arguments().contains( "--verify-invalid-command-repair" ) )
  {
    {
      QSettings run( runKey, QSettings::NativeFormat );
      run.setValue( settings.programName(), quoted + " --invalid-flag" );
      run.sync();
    }
    CHECK( preference() == 1 && !settings.hasStartOnSystemBoot() );
    CHECK( dialogAction( true, false, false, snapshots.isEmpty() ? QString() : snapshots+"/settings-invalid-command-repair.png" ) );
    CHECK( command() == quoted && settings.hasStartOnSystemBoot() );
    std::puts( "PASS re-enabling the actual settings checkbox repairs an invalid command even if the saved preference was already enabled" );
  }
  std::printf( "PASS actual settings dialog, opt-out, cancel, isolation, checkbox geometry at devicePixelRatio %.2f\n", window.devicePixelRatioF() );
  return 0;
}
