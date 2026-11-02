#ifndef SHADOWPREVIEWTRANSPORT_H
#define SHADOWPREVIEWTRANSPORT_H

#include <QObject>
#include <QTcpServer>

class ShadowPreviewTransport : public QObject
{
public:
  explicit ShadowPreviewTransport( QObject* parent );
  bool ready() const;
  bool exchange( const QByteArray& payload, QByteArray* received, QString* error );
  quint16 port() const { return m_server.serverPort(); }

private:
  QTcpServer m_server;
  QString m_token;
  bool m_exchanging = false;
};

#endif
