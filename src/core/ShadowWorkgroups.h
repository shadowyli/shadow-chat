#ifndef SHADOWWORKGROUPS_H
#define SHADOWWORKGROUPS_H

#include <QtCore>

namespace ShadowWorkgroups
{
inline QString marker() { return QStringLiteral( "shadowchat-workgroups-v1" ); }

inline bool normalize( const QStringList& input, QStringList* groups )
{
  groups->clear();
  if( input.size() > 32 ) return false;
  for( const QString& text : input )
  {
    if( text.contains( QRegularExpression( QStringLiteral( "[\\x00-\\x1f\\x7f\\x{2028}\\x{2029}]" ) ) ) )
      return false;
    const QString name = text.simplified();
    if( name.isEmpty() ) continue;
    if( name.size() > 128 || name.contains( QRegularExpression( QStringLiteral( "[\\x00-\\x1f\\x7f]" ) ) ) )
      return false;
    if( !groups->contains( name, Qt::CaseInsensitive ) ) groups->append( name );
  }
  return QJsonDocument( QJsonArray::fromStringList( *groups ) ).toJson( QJsonDocument::Compact ).size() <= 8192;
}

inline bool fromText( const QString& text, QStringList* groups )
{
  return normalize( text.split( QRegularExpression( QStringLiteral( "[,，、;；\\n]+" ) ),
    Qt::SkipEmptyParts ), groups );
}

inline bool fromJson( const QString& text, QStringList* groups )
{
  const QByteArray bytes = text.toUtf8();
  if( bytes.size() > 8192 ) return false;
  QJsonParseError error;
  const QJsonDocument document = QJsonDocument::fromJson( bytes, &error );
  if( error.error != QJsonParseError::NoError || !document.isArray() ) return false;
  QStringList input;
  for( const QJsonValue& value : document.array() )
  {
    if( !value.isString() ) return false;
    input.append( value.toString() );
  }
  return normalize( input, groups );
}
}

#endif
