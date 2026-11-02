#ifndef SHADOW_STICKER_MEDIA_H
#define SHADOW_STICKER_MEDIA_H

#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QRegularExpression>

inline bool shadowStickerFileName( const QString& path )
{
  static const QRegularExpression name(
    QStringLiteral( "^[a-f0-9]{64}(?: \\(\\d+\\))?\\.(?:png|jpe?g|gif|webp)$" ),
    QRegularExpression::CaseInsensitiveOption );
  return name.match( QFileInfo( path ).fileName() ).hasMatch();
}

inline bool shadowStoredSticker( const QString& path, const QString& accountFolder )
{
  const QFileInfo file( path );
  const QString library = QDir( accountFolder ).absoluteFilePath( QStringLiteral( "stickers" ) );
  return shadowStickerFileName( path ) &&
    QDir( file.absolutePath() ) == QDir( library );
}

inline bool shadowGifSource( const QString& path )
{
  if( QFileInfo( path ).suffix().compare( QStringLiteral( "gif" ), Qt::CaseInsensitive ) != 0 )
    return false;
  QImageReader reader( path );
  return reader.format() == QByteArrayLiteral( "gif" );
}

#endif
