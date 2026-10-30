#include <QtWidgets>
#define main previewProductionMain
#include "../src/desktop/Main.cpp"
#undef main

static bool coreStayedIsolated = false;
static bool devPropertyWasCleared = false;

static void scheduleRestartBoundaryTest()
{
  QTimer::singleShot( 1200, qApp, []() {
    coreStayedIsolated = Core::instance() && Core::instance()->devPreviewIsolated();
    qApp->setProperty( "shadowDevPreview", false );
    devPropertyWasCleared = !qApp->property( "shadowDevPreview" ).toBool();
    coreStayedIsolated = coreStayedIsolated && Core::instance()->devPreviewIsolated();
    qApp->setProperty( "shadowRestartForLogin", true );
    for( QWidget* widget : QApplication::topLevelWidgets() )
      if( widget->objectName() == "GuiMainWindow" )
        QMetaObject::invokeMethod( widget, "forceShutdown", Qt::QueuedConnection );
  } );
}
Q_COREAPP_STARTUP_FUNCTION( scheduleRestartBoundaryTest )

int main( int argc, char** argv )
{
  const QString proof = qEnvironmentVariable( "SHADOW_DEV_RESTART_PROOF" );
  if( proof.isEmpty() ) return 2;
  bool explicitDev = false;
  for( int index = 1; index < argc; ++index )
    if( QString::fromLocal8Bit( argv[index] ) == "--dev-preview" ) explicitDev = true;
  if( !explicitDev )
  {
    QFile unexpected( proof + ".unexpected-child" );
    if( unexpected.open( QIODevice::WriteOnly ) ) unexpected.write( "refused unisolated test child" );
    return 2;
  }
  qputenv( "QT_QPA_PLATFORM", "offscreen" );
  const int result = previewProductionMain( argc, argv );
  QJsonObject evidence;
  evidence["coreStayedIsolatedAfterPropertyFalse"] = coreStayedIsolated;
  evidence["dynamicPropertyWasCleared"] = devPropertyWasCleared;
  evidence["productionMainExitCode"] = result;
  QSaveFile file( proof );
  if( !file.open( QIODevice::WriteOnly ) ) return 3;
  file.write( QJsonDocument( evidence ).toJson() );
  if( !file.commit() ) return 3;
  return result == 0 && coreStayedIsolated && devPropertyWasCleared ? 0 : 1;
}
