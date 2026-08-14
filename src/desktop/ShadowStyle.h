//////////////////////////////////////////////////////////////////////
//
// 影聊 desktop design tokens and icon helpers.
//
//////////////////////////////////////////////////////////////////////

#ifndef SHADOWCHAT_SHADOWSTYLE_H
#define SHADOWCHAT_SHADOWSTYLE_H

#include <QColor>
#include <QFont>
#include <QIcon>
#include <QPixmap>
#include <QString>


namespace ShadowStyle
{
  QFont applicationFont();
  QString applicationStyleSheet();
  QPixmap tintedPixmap( const QString& resource_path, const QSize& size, const QColor& color );
  QIcon navigationIcon( const QString& name );
  QIcon actionIcon( const QString& name );
}

#endif // SHADOWCHAT_SHADOWSTYLE_H
