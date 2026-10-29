#include "ShadowMessageDelivery.h"
bool ShadowMessageDelivery::open( const QString& accountFolder )
{
  const QString path = QDir( accountFolder ).filePath( "message-delivery.json" );
  if( path == m_path ) return true;
  m_path = path; m_messages = QJsonObject();
  QFile file( path );
  if( !file.exists() ) return true;
  if( !file.open( QIODevice::ReadOnly ) ) { m_path.clear(); return false; }
  QJsonParseError error;
  const QJsonDocument document = QJsonDocument::fromJson( file.readAll(), &error );
  if( error.error != QJsonParseError::NoError || !document.isObject() ) { m_path.clear(); return false; }
  m_messages = document.object(); return true;
}
bool ShadowMessageDelivery::save()
{
  QSaveFile file( m_path );
  const QByteArray bytes = QJsonDocument( m_messages ).toJson();
  return file.open( QIODevice::WriteOnly ) && file.write( bytes ) == bytes.size() && file.commit();
}
bool ShadowMessageDelivery::begin( const QString& id, const QString& chat, const QStringList& recipients, const QString& preview )
{
  if( recipients.isEmpty() ) return false;
  if( m_messages.contains( id ) ) return true;
  QJsonObject message;
  message["chat"] = chat; message["preview"] = preview;
  message["recipients"] = QJsonArray::fromStringList( recipients );
  message["delivered"] = QJsonArray();
  m_messages[id] = message;
  if( save() ) return true;
  m_messages.remove( id ); return false;
}
ShadowMessageDelivery::Result ShadowMessageDelivery::acknowledge( const QString& id, const QString& recipient )
{
  Result result;
  if( !m_messages.contains( id ) ) return result;
  QJsonObject message = m_messages[id].toObject();
  const QJsonArray recipients = message["recipients"].toArray();
  QJsonArray delivered = message["delivered"].toArray();
  if( !recipients.contains( recipient ) || delivered.contains( recipient ) ) return result;
  delivered << recipient;
  result.chat = message["chat"].toString(); result.preview = message["preview"].toString();
  result.delivered = delivered.size(); result.total = recipients.size();
  const QJsonObject previous = message;
  if( delivered.size() == recipients.size() ) m_messages.remove( id );
  else { message["delivered"] = delivered; m_messages[id] = message; }
  if( save() ) result.changed = true;
  else m_messages[id] = previous;
  return result;
}
void ShadowMessageDelivery::cancel( const QString& chat, const QString& recipient )
{
  const QJsonObject previous = m_messages;
  foreach( const QString& id, m_messages.keys() )
  {
    QJsonObject message = m_messages[id].toObject();
    if( message["chat"].toString() != chat ) continue;
    if( recipient.isEmpty() ) { m_messages.remove( id ); continue; }
    QJsonArray remaining;
    foreach( const QJsonValue& value, message["recipients"].toArray() ) if( value.toString() != recipient ) remaining << value;
    QJsonArray delivered;
    foreach( const QJsonValue& value, message["delivered"].toArray() ) if( value.toString() != recipient ) delivered << value;
    if( remaining.size() <= delivered.size() ) m_messages.remove( id );
    else { message["recipients"] = remaining; message["delivered"] = delivered; m_messages[id] = message; }
  }
  if( !save() ) m_messages = previous;
}
