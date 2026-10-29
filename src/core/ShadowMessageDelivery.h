#ifndef SHADOW_MESSAGE_DELIVERY_H
#define SHADOW_MESSAGE_DELIVERY_H
#include <QtCore>
class ShadowMessageDelivery
{
public:
  struct Result { bool changed = false; QString chat, preview; int delivered = 0, total = 0; };
  bool open( const QString& accountFolder );
  bool begin( const QString& id, const QString& chat, const QStringList& recipients, const QString& preview );
  Result acknowledge( const QString& id, const QString& recipient );
  void cancel( const QString& chat, const QString& recipient = QString() );
private:
  bool save();
  QString m_path;
  QJsonObject m_messages;
};
#endif
