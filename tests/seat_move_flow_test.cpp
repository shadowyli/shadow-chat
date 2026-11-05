#include "BeeApplication.h"
#include "Core.h"
#include "GuiMain.h"
#include "IconManager.h"
#include "Random.h"
#include "Settings.h"
#include "ShadowDialogChrome.h"
#include "ShadowLocalAccounts.h"
#include "ShadowPopup.h"
#include <QtWidgets>
#include <QtTest>
#include <cstdio>

#define CHECK(value) do { if( !(value) ) { std::fprintf( stderr, "FAIL line %d: %s\n", __LINE__, #value ); return 1; } } while( false )

int main( int argc, char** argv )
{
  QApplication::setAttribute( Qt::AA_EnableHighDpiScaling );
  QApplication::setAttribute( Qt::AA_DontUseNativeDialogs );
  BeeApplication app( argc, argv );
  app.setQuitOnLastWindowClosed( false );
  ShadowDialogChrome::install( &app );
  QTemporaryDir fixture;
  CHECK( fixture.isValid() );
  const QString root = fixture.filePath( "accounts" );
  app.setProperty( "shadowAccountRoot", root );
  const QString email = "seat-ui@example.test";
  ShadowLocalAccounts accounts( root );
  QString error;
  CHECK( accounts.create( email, "synthetic-seat-password", &error ) );
  accounts.rememberLogin( email );
  const QString profile = accounts.profileFolder( email );
  Random::init();
  Settings& settings = Settings::instance();
  settings.setDefaultFolders();
  settings.createLocalUser( "synthetic-seat-test" );
  settings.setAccountDataFolder( profile );
  settings.load();
  IconManager::instance();
  Core core;
  ShadowPopup::installMenuTracking();
  GuiMain window;
  window.loadStyle();
  window.show();
  QTest::qWait( 60 );
  QAction* action = window.findChild<QAction*>( "ShadowSeatMoveAction" );
  CHECK( action && action->isEnabled() );
  const QString proof = qEnvironmentVariable( "SHADOWCHAT_UI_PROOF_DIR" );
  if( !proof.isEmpty() )
  {
    QDir().mkpath( proof );
    QMenu* menu = window.findChild<QMenu*>( "ShadowMainMenu" );
    CHECK( menu );
    menu->popup( window.mapToGlobal( QPoint( 30, 40 ) ) );
    QTest::qWait( 30 );
    CHECK( menu->grab().save( QDir( proof ).filePath( "seat-move-main-menu.png" ) ) );
    menu->close();
  }
  const QString successPath = fixture.filePath( "seat-history.zip" );
  auto run = [&]( const QString& mode ) {
    bool confirmation = false, fileDialog = false, result = false, defaultName = false;
    QElapsedTimer elapsed;
    elapsed.start();
    QTimer actor;
    QObject::connect( &actor, &QTimer::timeout, &window, [&]() {
      if( elapsed.elapsed() > 15000 ) qFatal( "Seat move dialog timed out" );
      QWidget* modal = QApplication::activeModalWidget();
      if( !modal ) return;
      if( modal->objectName() == "ShadowConfirmationDialog" )
      {
        confirmation = true;
        QLabel* description = modal->findChild<QLabel*>( "ShadowConfirmationText" );
        if( !description || !description->text().contains( QStringLiteral( "本机历史保留" ) ) ||
            !description->text().contains( QStringLiteral( "ZIP 未加密" ) ) )
          qFatal( "Missing cleanup scope and backup warning" );
        if( !proof.isEmpty() )
          modal->grab().save( QDir( proof ).filePath( "seat-move-confirmation.png" ) );
        qobject_cast<QDialog*>( modal )->done( mode == "cancel-confirmation" ? QDialog::Rejected : QDialog::Accepted );
      }
      else if( QFileDialog* selector = qobject_cast<QFileDialog*>( modal ) )
      {
        fileDialog = true;
        defaultName = !selector->selectedFiles().isEmpty() &&
          QFileInfo( selector->selectedFiles().first() ).fileName().startsWith(
            QStringLiteral( "影聊_换座位历史记录_seat-ui@example.test_" ) );
        if( mode == "cancel-file" ) selector->reject();
        else
        {
          const QString destination = mode == "export-failure" ? QDir( profile ).filePath( "unsafe.zip" ) : successPath;
          selector->setDirectory( QFileInfo( destination ).absolutePath() );
          selector->selectFile( QFileInfo( destination ).fileName() );
          QLineEdit* filename = selector->findChild<QLineEdit*>( "fileNameEdit" );
          if( filename ) filename->setText( QFileInfo( destination ).fileName() );
          QMetaObject::invokeMethod( selector, "accept", Qt::DirectConnection );
        }
      }
      else if( QMessageBox* message = qobject_cast<QMessageBox*>( modal ) )
      {
        const QString expected = mode == "export-failure" ?
          QStringLiteral( "换座位备份失败" ) : QStringLiteral( "换座位备份已完成" );
        result = message->windowTitle() == expected;
        if( !result ) qFatal( "Unexpected seat move result" );
        if( mode == "success" ) std::printf( "seat move result: %s\n", qPrintable( message->text() ) );
        message->accept();
      }
    } );
    actor.start( 20 );
    action->trigger();
    actor.stop();
    return confirmation && (mode == "cancel-confirmation" ||
      (fileDialog && defaultName && (mode == "cancel-file" || result)));
  };
  CHECK( run( "cancel-confirmation" ) );
  CHECK( accounts.authenticate( email, "synthetic-seat-password" ) && accounts.autoLoginEmail() == email );
  CHECK( !app.property( "shadowRestartForLogin" ).toBool() && window.isVisible() );
  std::puts( "PASS confirmation cancel preserves password and current session" );
  CHECK( run( "cancel-file" ) );
  CHECK( accounts.authenticate( email, "synthetic-seat-password" ) && accounts.autoLoginEmail() == email );
  CHECK( !app.property( "shadowRestartForLogin" ).toBool() && window.isVisible() );
  std::puts( "PASS file dialog cancel preserves password and current session" );
  CHECK( run( "export-failure" ) );
  CHECK( accounts.authenticate( email, "synthetic-seat-password" ) && accounts.autoLoginEmail() == email );
  CHECK( !QFileInfo::exists( QDir( profile ).filePath( "unsafe.zip" ) ) && action->isEnabled() );
  CHECK( !app.property( "shadowRestartForLogin" ).toBool() && window.isVisible() );
  std::puts( "PASS export failure does not clear password or exit and permits retry" );
  CHECK( run( "success" ) );
  std::printf( "seat move saved bytes=%lld account-exists=%d\n",
    QFileInfo( successPath ).size(), accounts.exists( email ) );
  CHECK( QFileInfo( successPath ).size() > 0 && accounts.exists( email ) );
  CHECK( !accounts.authenticate( email, "synthetic-seat-password" ) && accounts.autoLoginEmail().isEmpty() );
  CHECK( !accounts.autoLoginEnabled() && app.property( "shadowRestartForLogin" ).toBool() && !window.isVisible() );
  CHECK( QFileInfo::exists( QDir( profile ).filePath( "beebeep.dat" ) ) );
  std::puts( "PASS actual menu exports ZIP before clearing local password and exits to login with history retained" );
  return 0;
}
