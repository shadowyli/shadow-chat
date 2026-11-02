#ifndef SHADOWAVATARINTERACTIONS_H
#define SHADOWAVATARINTERACTIONS_H

#include "Chat.h"
#include "User.h"

namespace ShadowAvatarInteractions
{
  VNumber userIdFromLink( const QUrl& );
  User userForChat( VNumber, const Chat& );
  QRect popupGeometry( const QPoint&, const QSize&, const QRect& );
  QRect conversationAvatarRect( const QRect& );
}

#endif
