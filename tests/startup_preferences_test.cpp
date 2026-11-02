#include "BeeApplication.h"
#include "Core.h"
#include "GuiMain.h"
#include "Settings.h"
#include "Random.h"
#include <QtWidgets>
#include <cstdio>
#include <windows.h>

#define CHECK(value) do { if( !(value) ) { std::fprintf( stderr, "FAIL line %d: %s\n", __LINE__, #value ); return 1; } } while(false)

class RegistryFixture
{
public:
  RegistryFixture() : handle( nullptr ), active( false )
  {
    name = QStringLiteral( "Software\\ShadowChatStartupTest-%1" ).arg( QUuid::createUuid().toString( QUuid::WithoutBraces ) );
    if( RegCreateKeyExW( HKEY_CURRENT_USER, reinterpret_cast<LPCWSTR>( name.utf16() ), 0, nullptr, 0,
                        KEY_ALL_ACCESS, nullptr, &handle, nullptr ) == ERROR_SUCCESS )
      active = RegOverridePredefKey( HKEY_CURRENT_USER, handle ) == ERROR_SUCCESS;
  }
  ~RegistryFixture()
  {
    if( active ) RegOverridePredefKey( HKEY_CURRENT_USER, nullptr );
    if( handle ) RegCloseKey( handle );
    RegDeleteTreeW( HKEY_CURRENT_USER, reinterpret_cast<LPCWSTR>( name.utf16() ) );
  }
  bool valid() const { return active; }
private:
  QString name;
  HKEY handle;
  bool active;
};

int main( int argc, char** argv )
{
  BeeApplication application( argc, argv );
  RegistryFixture registry;
  CHECK( registry.valid() );
  QTemporaryDir temporary;
  CHECK( temporary.isValid() );
  Random::init();
  Settings& settings = Settings::instance();
  settings.setDefaultFolders();
  settings.createLocalUser( "startup-fixture" );
  settings.setAccountDataFolder( temporary.path() );
  settings.load();
  const QString preferenceKey = QStringLiteral( "HKEY_CURRENT_USER\\Software\\shadowyli\\ShadowChat" );
  const QString runKey = QStringLiteral( "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run" );
  auto preference = [&]() { return QSettings( preferenceKey, QSettings::NativeFormat ).value( "StartupEnabled", -1 ).toInt(); };
  auto command = [&]() { return QSettings( runKey, QSettings::NativeFormat ).value( settings.programName() ).toString(); };
  {
    QSettings run( runKey, QSettings::NativeFormat );
    run.setValue( "UnrelatedFixture", "preserve-me" );
    run.sync();
  }
  CHECK( settings.configureStartOnSystemBoot( false ) );
  CHECK( settings.hasStartOnSystemBoot() && preference() == 1 );
  CHECK( command() == QStringLiteral( "\"%1\"" ).arg( QDir::toNativeSeparators( application.applicationFilePath() ) ) );
  CHECK( settings.removeStartOnSystemBoot() );
  CHECK( !settings.hasStartOnSystemBoot() && preference() == 0 && command().isEmpty() );
  CHECK( settings.configureStartOnSystemBoot( false ) );
  CHECK( settings.configureStartOnSystemBoot( true ) );
  CHECK( command().isEmpty() && preference() == 0 );
  {
    QSettings prior( preferenceKey, QSettings::NativeFormat );
    prior.remove( "StartupEnabled" );
    prior.sync();
  }
  CHECK( settings.configureStartOnSystemBoot( true ) );
  CHECK( command().isEmpty() && preference() == 0 );
  application.setProperty( "shadowAccountRoot", temporary.path() );
  CHECK( !settings.addStartOnSystemBoot() && !settings.removeStartOnSystemBoot() );
  CHECK( !settings.configureStartOnSystemBoot( false ) && command().isEmpty() );
  application.setProperty( "shadowAccountRoot", QVariant() );
  application.setProperty( "shadowDevPreview", true );
  CHECK( !settings.addStartOnSystemBoot() && !settings.configureStartOnSystemBoot( false ) );
  application.setProperty( "shadowDevPreview", false );
  CHECK( settings.addStartOnSystemBoot() );
  Core core;
  GuiMain window;
  bool changed = false;
  QTimer::singleShot( 100, [&]() {
    QDialog* dialog = qobject_cast<QDialog*>( QApplication::activeModalWidget() );
    if( !dialog ) return;
    QCheckBox* startup = dialog->findChild<QCheckBox*>( "ShadowStartupCheck" );
    if( startup && startup->isEnabled() && startup->isChecked() )
    {
      startup->setChecked( false );
      changed = true;
    }
    dialog->accept();
  } );
  CHECK( QMetaObject::invokeMethod( &window, "showShadowSettings", Qt::DirectConnection ) );
  CHECK( changed && command().isEmpty() && preference() == 0 );
  CHECK( settings.configureStartOnSystemBoot( false ) && command().isEmpty() );
  CHECK( QSettings( runKey, QSettings::NativeFormat ).value( "UnrelatedFixture" ) == "preserve-me" );
  std::puts( "PASS startup preferences: quoted HKCU Run, default enable, shared explicit opt-out, legacy opt-out, native settings disable, repeated login respects preference, isolated profile/dev-preview guards, unrelated Run entry preserved; all registry writes redirected to disposable fixture" );
  return 0;
}
