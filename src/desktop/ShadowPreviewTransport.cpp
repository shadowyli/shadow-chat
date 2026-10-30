#include "ShadowPreviewTransport.h"

#include <QCryptographicHash>
#include <QDataStream>
#include <QEventLoop>
#include <QHostAddress>
#include <QScopedValueRollback>
#include <QTcpSocket>
#include <QTimer>
#include <QUuid>
#include <QVariant>

namespace
{
const int maximumPayload = 16 * 1024 * 1024;
const int maximumFrame = maximumPayload + 1024;

QByteArray frame( const QByteArray& data )
{
  QByteArray result;
  QDataStream stream( &result, QIODevice::WriteOnly );
  stream << quint32( data.size() );
  result.append( data );
  return result;
}

int readFrame( QByteArray* buffer, QByteArray* result )
{
  if( buffer->size() < 4 ) return 0;
  QDataStream stream( *buffer );
  quint32 length = 0;
  stream >> length;
  if( length > maximumFrame ) return -1;
  if( buffer->size() < int( length ) + 4 ) return 0;
  *result = buffer->mid( 4, int( length ) );
  buffer->remove( 0, int( length ) + 4 );
  return 1;
}
}

ShadowPreviewTransport::ShadowPreviewTransport( QObject* parent )
  : QObject( parent ), m_server( this ), m_token( QUuid::createUuid().toString() )
{
  setObjectName( QStringLiteral( "ShadowPreviewTransport" ) );
  m_server.setObjectName( QStringLiteral( "ShadowPreviewPeerServer" ) );
  setProperty( "receivedMessages", 0 );
  setProperty( "receivedBytes", 0 );
  connect( &m_server, &QTcpServer::newConnection, this, [this]() {
    while( m_server.hasPendingConnections() )
    {
      QTcpSocket* socket = m_server.nextPendingConnection();
      if( !socket->peerAddress().isLoopback() ) { socket->deleteLater(); continue; }
      socket->setReadBufferSize( maximumFrame + 4 );
      connect( socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater );
      QTimer::singleShot( 5000, socket, [socket]() { socket->abort(); } );
      connect( socket, &QTcpSocket::readyRead, socket, [this, socket]() {
        if( socket->property( "handled" ).toBool() ) return;
        QByteArray bytes = socket->property( "input" ).toByteArray();
        bytes.append( socket->readAll() );
        QByteArray packet;
        const int status = readFrame( &bytes, &packet );
        if( status < 0 ) { socket->abort(); return; }
        socket->setProperty( "input", bytes );
        if( !status ) return;
        const QByteArray token = m_token.toUtf8();
        const QByteArray payload = packet.mid( token.size() );
        if( !packet.startsWith( token ) || payload.size() > maximumPayload )
        {
          socket->abort();
          return;
        }
        socket->setProperty( "handled", true );
        setProperty( "receivedMessages", property( "receivedMessages" ).toInt() + 1 );
        setProperty( "receivedBytes", property( "receivedBytes" ).toLongLong() + payload.size() );
        const QByteArray reply = token + QCryptographicHash::hash( payload, QCryptographicHash::Sha256 ) + payload;
        socket->write( frame( reply ) );
        socket->disconnectFromHost();
      } );
    }
  } );
  m_server.listen( QHostAddress::LocalHost, 0 );
}

bool ShadowPreviewTransport::ready() const
{
  return m_server.isListening() && m_server.serverAddress().isLoopback();
}

bool ShadowPreviewTransport::exchange( const QByteArray& payload, QByteArray* received, QString* error )
{
  if( !ready() || m_exchanging )
  {
    *error = QStringLiteral( "未发送：本机测试对端未就绪，请重启预览后重试。" );
    return false;
  }
  if( payload.size() > maximumPayload )
  {
    *error = QStringLiteral( "未发送：本机测试单次数据不能超过 16 MB。" );
    return false;
  }
  QScopedValueRollback<bool> exchanging( m_exchanging, true );
  QEventLoop loop;
  QTcpSocket client;
  client.setReadBufferSize( maximumFrame + 4 );
  QTimer timeout;
  timeout.setSingleShot( true );
  bool accepted = false;
  QByteArray buffer;
  connect( &timeout, &QTimer::timeout, &loop, &QEventLoop::quit );
  connect( &client, QOverload<QAbstractSocket::SocketError>::of( &QTcpSocket::error ),
    &loop, &QEventLoop::quit );
  connect( &client, &QTcpSocket::connected, &loop, [&]() {
    const QByteArray packet = m_token.toUtf8() + payload;
    client.write( frame( packet ) );
  } );
  connect( &client, &QTcpSocket::readyRead, &loop, [&]() {
    buffer.append( client.readAll() );
    QByteArray packet;
    const int status = readFrame( &buffer, &packet );
    if( status < 0 ) { loop.quit(); return; }
    if( !status ) return;
    const QByteArray token = m_token.toUtf8();
    const QByteArray checksum = packet.mid( token.size(), 32 );
    const QByteArray delivered = packet.mid( token.size() + 32 );
    accepted = packet.startsWith( token ) && delivered == payload &&
      checksum == QCryptographicHash::hash( delivered, QCryptographicHash::Sha256 );
    if( accepted ) *received = delivered;
    loop.quit();
  } );
  timeout.start( 3000 );
  client.connectToHost( QHostAddress::LocalHost, m_server.serverPort() );
  loop.exec( QEventLoop::ExcludeUserInputEvents );
  client.abort();
  if( !accepted ) *error = QStringLiteral( "未发送：本机测试发送失败或未收到回执，内容已保留。" );
  return accepted;
}
