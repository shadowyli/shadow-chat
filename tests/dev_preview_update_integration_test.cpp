#include <QtWidgets>
#include <QTcpServer>
#include <QTcpSocket>
#define main previewProductionMain
#include "../src/desktop/Main.cpp"
#undef main

static bool serverListening = false;
static bool isolatedReady = false;
static bool manualStarted = false;
static bool manualFinished = false;
static int earlyConnections = 0;
static int manualConnections = 0;
static int quietWindowMs = 92000;
static QByteArray requestLine;

static void stopPreview()
{
  for( QWidget* widget : QApplication::topLevelWidgets() )
    if( widget->objectName() == "GuiMainWindow" )
      QMetaObject::invokeMethod( widget, "forceShutdown", Qt::QueuedConnection );
}

static void scheduleUpdateIntegrationTest()
{
  const int requestedWindow = qEnvironmentVariableIntValue( "SHADOW_DEV_UPDATE_WAIT_MS" );
  if( requestedWindow > 0 ) quietWindowMs = requestedWindow;
  QTcpServer* server = new QTcpServer( qApp );
  const QUrl source = ShadowDevPreview::previewUpdateSource();
  serverListening = server->listen( QHostAddress::LocalHost, source.port() );
  QObject::connect( server, &QTcpServer::newConnection, server, [server]() {
    while( QTcpSocket* socket = server->nextPendingConnection() )
    {
      if( manualStarted ) ++manualConnections;
      else ++earlyConnections;
      QObject::connect( socket, &QTcpSocket::readyRead, socket, [socket]() {
        QByteArray data = socket->property( "request" ).toByteArray() + socket->readAll();
        socket->setProperty( "request", data );
        if( !data.contains( "\r\n\r\n" ) ) return;
        requestLine = data.left( data.indexOf( "\r\n" ) );
        socket->write( "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\n{}" );
        socket->disconnectFromHost();
      } );
    }
  } );
  QTimer::singleShot( quietWindowMs, qApp, [server]() {
    ShadowUpdateController* controller = nullptr;
    for( QWidget* widget : QApplication::topLevelWidgets() )
      if( widget->objectName() == "GuiMainWindow" )
      {
        isolatedReady = widget->isVisible() && ShadowDevPreview::active() &&
          Core::instance() && Core::instance()->devPreviewIsolated();
        controller = widget->findChild<ShadowUpdateController*>();
      }
    if( !serverListening || !isolatedReady || !controller || earlyConnections )
    {
      stopPreview();
      return;
    }
    QObject::connect( controller, &ShadowUpdateController::checkFinished, server, []( bool ) {
      manualFinished = true;
      stopPreview();
    } );
    manualStarted = true;
    controller->checkManuallyInline();
    QTimer::singleShot( 5000, server, []() { stopPreview(); } );
  } );
}
Q_COREAPP_STARTUP_FUNCTION( scheduleUpdateIntegrationTest )

int main( int argc, char** argv )
{
  const QString proof = qEnvironmentVariable( "SHADOW_DEV_UPDATE_PROOF" );
  if( proof.isEmpty() ) return 2;
  qputenv( "QT_QPA_PLATFORM", "offscreen" );
  const int result = previewProductionMain( argc, argv );
  QJsonObject evidence;
  evidence["productionMainExitCode"] = result;
  evidence["serverListening"] = serverListening;
  evidence["isolatedReady"] = isolatedReady;
  evidence["earlyConnections"] = earlyConnections;
  evidence["quietWindowMs"] = quietWindowMs;
  evidence["manualConnections"] = manualConnections;
  evidence["manualFinished"] = manualFinished;
  evidence["requestLine"] = QString::fromLatin1( requestLine );
  QSaveFile file( proof );
  if( !file.open( QIODevice::WriteOnly ) ) return 3;
  file.write( QJsonDocument( evidence ).toJson() );
  if( !file.commit() ) return 3;
  return result == 0 && serverListening && isolatedReady && earlyConnections == 0 &&
    manualConnections == 1 && manualFinished &&
    requestLine == "GET /updates/shadowchat.json HTTP/1.1" ? 0 : 1;
}
