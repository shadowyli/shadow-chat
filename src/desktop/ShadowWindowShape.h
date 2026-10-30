#ifndef SHADOWWINDOWSHAPE_H
#define SHADOWWINDOWSHAPE_H

#include <QRegion>
#include <QSize>
#include <QtMath>

namespace ShadowWindowShape
{
const int cornerRadius = 12;

inline QRegion roundedMask( const QSize& size )
{
  if( size.isEmpty() ) return QRegion();
  const int radius = qMin( cornerRadius, qMin( size.width(), size.height() ) / 2 );
  QRegion region( 0, radius, size.width(), size.height() - radius * 2 );
  for( int row = 0; row < radius; ++row )
  {
    const qreal distance = radius - row - 0.5;
    const int inset = qMax( 0, qCeil( radius - qSqrt( radius * radius - distance * distance ) - 0.5 ) );
    const int width = size.width() - inset * 2;
    region += QRect( inset, row, width, 1 );
    region += QRect( inset, size.height() - row - 1, width, 1 );
  }
  return region;
}
}

#endif
