#include "ShadowAvatarInteractions.h"
#include "UserManager.h"
#include <QRegularExpression>

VNumber ShadowAvatarInteractions::userIdFromLink( const QUrl& url )
{
  static const QRegularExpression pattern(
    QStringLiteral( "^shadowuser:([1-9][0-9]*)(?:\\?name=[A-Za-z0-9_-]*)?$" ),
    QRegularExpression::CaseInsensitiveOption );
  if( !url.isValid() )
    return ID_INVALID;
  const QRegularExpressionMatch match = pattern.match( QString::fromLatin1( url.toEncoded() ) );
  if( !match.hasMatch() )
    return ID_INVALID;
  bool valid = false;
  const VNumber userId = match.captured( 1 ).toULongLong( &valid );
  return valid && userId != ID_SYSTEM_MESSAGE && userId != ID_DEFAULT_CHAT ? userId : ID_INVALID;
}

User ShadowAvatarInteractions::userForChat( VNumber userId, const Chat& chat )
{
  if( !chat.isValid() || userId == ID_INVALID || userId == ID_SYSTEM_MESSAGE || userId == ID_DEFAULT_CHAT )
    return User();
  const User user = UserManager::instance().findUser( userId );
  if( !user.isValid() || user.id() != userId )
    return User();
  if( user.isLocal() || chat.hasUser( userId ) )
    return user;
  foreach( const ChatMessage& message, chat.messages() )
    if( message.userId() == userId && !message.isFromSystem() )
      return user;
  return User();
}

QRect ShadowAvatarInteractions::popupGeometry( const QPoint& anchor, const QSize& requested, const QRect& available )
{
  const QSize size( qMin( requested.width(), available.width() ), qMin( requested.height(), available.height() ) );
  const int horizontal = qBound( available.left(), anchor.x(), available.right() - size.width() + 1 );
  const int vertical = qBound( available.top(), anchor.y(), available.bottom() - size.height() + 1 );
  return QRect( QPoint( horizontal, vertical ), size );
}

QRect ShadowAvatarInteractions::conversationAvatarRect( const QRect& item )
{
  const QRect row = item.adjusted( 8, 3, -8, -3 );
  return QRect( row.left() + 10, row.center().y() - 23, 46, 46 );
}
