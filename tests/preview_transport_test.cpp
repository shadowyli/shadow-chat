#include "ShadowPreviewTransport.h"
#include <QCoreApplication>
#include <QDataStream>
#include <QEventLoop>
#include <QHostAddress>
#include <QTcpSocket>
#include <QTimer>
#include <QVariant>
#include <cstdio>

#define CHECK(value) do { if( !(value) ) { std::fprintf( stderr, "FAIL line %d: %s\n", __LINE__, #value ); return 1; } } while(false)

int main( int argc, char** argv )
{
  QCoreApplication application( argc, argv );
  std::setvbuf( stdout, Q_NULLPTR, _IONBF, 0 );
  std::puts( "Transport application ready" );
  ShadowPreviewTransport transport( &application );
  std::puts( "Transport listener ready" );
  QTcpServer* server = transport.findChild<QTcpServer*>( "ShadowPreviewPeerServer" );
  CHECK( transport.ready() && server && server->serverAddress() == QHostAddress::LocalHost );
  QByteArray binary;
  for( int value = 0; value < 256; ++value ) binary.append( char( value ) );
  binary = binary.repeated( 800 );
  QString error;
  QByteArray received;
  CHECK( transport.exchange( binary, &received, &error ) && received == binary );
  std::puts( "Binary receipt verified" );
  CHECK( transport.property( "receivedMessages" ).toInt() == 1 );
  CHECK( transport.property( "receivedBytes" ).toInt() == binary.size() );
  const QByteArray unicode = QString::fromUtf8( "1234564 发送测试" ).toUtf8();
  CHECK( transport.exchange( unicode, &received, &error ) && received == unicode );
  std::puts( "Unicode receipt verified" );
  CHECK( !transport.exchange( QByteArray( 16 * 1024 * 1024 + 1, 'x' ), &received, &error ) );
  CHECK( transport.property( "receivedMessages" ).toInt() == 2 );
  const QList<QByteArray> rejectedPackets = QList<QByteArray>() << QByteArray::fromHex( "ffffffff" ) <<
    QByteArray::fromHex( "00000005" ) + QByteArray( "wrong" );
  for( const QByteArray& packet : rejectedPackets )
  {
    QTcpSocket outsider;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot( true );
    QObject::connect( &timeout, &QTimer::timeout, &loop, &QEventLoop::quit );
    QObject::connect( &outsider, &QTcpSocket::disconnected, &loop, &QEventLoop::quit );
    QObject::connect( &outsider, &QTcpSocket::connected, &loop, [&]() { outsider.write( packet ); } );
    timeout.start( 1500 );
    outsider.connectToHost( QHostAddress::LocalHost, transport.port() );
    loop.exec();
    std::puts( "Rejected frame loop exited" );
    CHECK( outsider.state() == QAbstractSocket::UnconnectedState );
  }
  CHECK( transport.property( "receivedMessages" ).toInt() == 2 );
  server->close();
  CHECK( !transport.ready() && !transport.exchange( unicode, &received, &error ) );
  CHECK( error.contains( QString::fromUtf8( "未就绪" ) ) );
  std::puts( "PASS local TCP test peer: loopback listener, binary/unicode receipt, byte counts, size limit, unauthenticated/oversize frames rejected, disconnected peer fails without accepting data" );
  return 0;
}
