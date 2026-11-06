#ifndef SHADOW_VIDEO_CARD_H
#define SHADOW_VIDEO_CARD_H

#include <QPainter>
#include <QPainterPath>
#include <QSaveFile>
#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include "ShadowStyle.h"

inline QPixmap shadowVideoCardPixmap( const QSize& size )
{
  QPixmap image( size );
  image.fill( QColor( "#263445" ) );
  QPainter painter( &image );
  painter.setRenderHint( QPainter::Antialiasing );
  const QPointF center( size.width() / 2.0, size.height() / 2.0 );
  const qreal radius = qMin( size.width(), size.height() ) * .18;
  painter.setPen( Qt::NoPen );
  painter.setBrush( QColor( 255, 255, 255, 38 ) );
  painter.drawEllipse( center, radius, radius );
  QPainterPath play;
  play.moveTo( center + QPointF( -radius * .25, -radius * .45 ) );
  play.lineTo( center + QPointF( radius * .5, 0 ) );
  play.lineTo( center + QPointF( -radius * .25, radius * .45 ) );
  play.closeSubpath();
  painter.fillPath( play, Qt::white );
  QFont font = ShadowStyle::applicationFont();
  font.setPixelSize( 12 );
  painter.setFont( font );
  painter.setPen( QColor( "#DBE4EF" ) );
  painter.drawText( image.rect().adjusted( 12, 8, -12, -8 ), Qt::AlignLeft | Qt::AlignBottom,
    QStringLiteral( "视频" ) );
  return image;
}

inline QString shadowVideoCardHtml( const QUrl& url, const QString& cacheFolder )
{
  const QString path = QDir( cacheFolder ).filePath( QStringLiteral( "video-preview-card-v1.png" ) );
  if( !QFileInfo::exists( path ) )
  {
    if( !QDir().mkpath( cacheFolder ) )
      return QStringLiteral( "<a href=\"%1\">视频 · 点击播放</a>" ).arg( url.toString().toHtmlEscaped() );
    QSaveFile file( path );
    if( !file.open( QIODevice::WriteOnly ) || !shadowVideoCardPixmap( QSize( 224, 126 ) ).save( &file, "PNG" ) ||
        !file.commit() )
      return QStringLiteral( "<a href=\"%1\">视频 · 点击播放</a>" ).arg( url.toString().toHtmlEscaped() );
  }
  return QStringLiteral( "<!--shadow-video--><a href=\"%1\"><img src=\"%2\" width=\"224\" height=\"126\" alt=\"视频 · 点击播放\"></a>" )
    .arg( url.toString().toHtmlEscaped(), QUrl::fromLocalFile( path ).toString().toHtmlEscaped() );
}

#endif
