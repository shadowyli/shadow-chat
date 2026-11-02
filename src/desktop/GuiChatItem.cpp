//////////////////////////////////////////////////////////////////////
//
// BeeBEEP Copyright (C) 2010-2021 Marco Mastroddi
//
// BeeBEEP is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published
// by the Free Software Foundation, either version 3 of the License,
// or (at your option) any later version.
//
// BeeBEEP is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with BeeBEEP. If not, see <http://www.gnu.org/licenses/>.
//
// Author: Marco Mastroddi <marco.mastroddi(AT)gmail.com>
//
// $Id: GuiChatItem.cpp 1550 2023-02-01 08:50:11Z mastroddi $
//
//////////////////////////////////////////////////////////////////////

#include "GuiChatItem.h"
#include "Chat.h"
#include "ChatManager.h"
#include "IconManager.h"
#include "MessageManager.h"
#include "Settings.h"
#include "ShadowStyle.h"
#include "UserManager.h"
#include "ShadowConversationPreferences.h"
#include "ShadowMention.h"


GuiChatItem::GuiChatItem( QTreeWidget* parent )
 : QTreeWidgetItem( parent )
{
  setData( 0, ChatId, 0 );
  setData( 0, ChatName, "" );
  setData( 0, ChatIsGroup, false );
  setData( 0, ChatUnreadMessages, 0 );
  setData( 0, ChatMessages, 0 );
  setData( 0, ChatLastMessageTimestamp, QDateTime( QDate( 1975, 4, 6), QTime( 7, 15 ) ) );
}

bool GuiChatItem::operator<( const QTreeWidgetItem& item ) const
{
  const bool pinned=data(0,ChatPinned).toBool();
  const bool other_pinned=item.data(0,ChatPinned).toBool();
  if(pinned!=other_pinned)return !pinned;
  if( chatId() == ID_DEFAULT_CHAT )
    return false;

  if( Bee::qVariantToVNumber( item.data( 0, GuiChatItem::ChatId ) ) == ID_DEFAULT_CHAT )
    return true;

  QDateTime chat_timestamp = lastMessageTimestamp();
  QDateTime other_timestamp = item.data( 0, GuiChatItem::ChatLastMessageTimestamp ).toDateTime();

  if( chat_timestamp != other_timestamp )
    return chat_timestamp < other_timestamp;

  int chat_messages = data( 0, GuiChatItem::ChatMessages ).toInt();
  int other_messages = item.data( 0, GuiChatItem::ChatMessages ).toInt();
  if( chat_messages != other_messages )
    return chat_messages < other_messages;

  int chat_online_users = onlineUsers();
  int other_online_users = item.data( 0, GuiChatItem::ChatOnlineUsers ).toInt();
  if( chat_online_users != other_online_users )
    return chat_online_users < other_online_users;

  return data( 0, GuiChatItem::ChatName ).toString().toLower() < item.data( 0, GuiChatItem::ChatName ).toString().toLower();
}

QString GuiChatItem::defaultChatName()
{
  return QStringLiteral( "全员群" );
}

bool GuiChatItem::updateItem( const Chat& c )
{
  QString chat_name;
  QString tool_tip;
  m_defaultIcon = QIcon();
  int unsent_messages = MessageManager::instance().countMessagesToSendInChatId( c.id() );
  int online_users = 0;

  if( c.isDefault() )
  {
    chat_name = defaultChatName();
    tool_tip = QObject::tr( "Open chat with all users" );
    setData( 0, ChatName, " " );
    m_defaultIcon = ShadowStyle::actionIcon( QStringLiteral( "users-three" ) );
  }
  else
  {
    UserList user_list = UserManager::instance().userList().fromUsersId( c.usersId() );
    QStringList sl;
    foreach( User u, user_list.toList() )
    {
      if( !u.isLocal() && u.isValid() )
      {
        QString user_name = Bee::userNameToShow( u, false );
        sl.append( user_name );
        if( c.isPrivateForUser( u.id() ) )
          m_defaultIcon = Bee::avatarForUser( u, QSize( 48, 48 ), Settings::instance().showUserPhoto() );
        if( u.isStatusConnected() )
          online_users++;
      }
    }

    if( c.isGroup() )
    {
      chat_name = c.name();
      QPixmap groupAvatar( 48, 48 );
      groupAvatar.fill( Qt::transparent );
      QPainter painter( &groupAvatar );
      painter.setRenderHint( QPainter::Antialiasing );
      painter.setPen( Qt::NoPen );
      painter.setBrush( QColor( "#7388F4" ) );
      painter.drawEllipse( groupAvatar.rect().adjusted( 1, 1, -1, -1 ) );
      painter.drawPixmap( 10, 10, ShadowStyle::tintedPixmap(
        QStringLiteral( ":/images/shadowchat/icons/users-three.svg" ), QSize( 28, 28 ), Qt::white ) );
      m_defaultIcon = QIcon( groupAvatar );
    }
    else
      chat_name = sl.isEmpty() ? c.name() : sl.first();

    tool_tip = QObject::tr( "Open chat with %1" ).arg( chat_name );

    setData( 0, ChatName, chat_name );
    setIsGroup( c.isGroup() );
  }

  if( unsent_messages > 0 )
    tool_tip += QString( "\n[%1 %2]" ).arg( unsent_messages ).arg( QObject::tr( "unsent messages" ) );

  const ShadowConversationPreferences preferences;
  const bool pinned=preferences.pinned(c),muted=preferences.muted(c);
  setData(0,ChatPinned,pinned);setData(0,ChatMuted,muted);
  if(pinned) tool_tip+=QStringLiteral("\n已置顶");
  if(muted) tool_tip+=QStringLiteral("\n消息免打扰（被 @ 时仍提醒）");
  const bool pending = preferences.pending( c );
  setData( 0, ChatPending, pending );
  setData( 0, ChatQueued, unsent_messages );
  QString preview;
  QDateTime latestTimestamp = c.lastMessageTimestamp();
  bool mentioned = false;
  int unreadRemaining = c.unreadMessages();
  const QList<ChatMessage>& messages = c.messages();
  for( int index = messages.size() - 1; index >= 0; --index )
  {
    const ChatMessage& message = messages.at( index );
    if( message.isHeader() || message.isSystemActivity() ) continue;
    QTextDocument text;
    text.setHtml( message.message() );
    const QString plain = text.toPlainText().remove( QChar::ObjectReplacementCharacter ).simplified();
    if( preview.isEmpty() )
    {
      if( message.timestamp().isValid() && (!latestTimestamp.isValid() || message.timestamp() > latestTimestamp) )
        latestTimestamp = message.timestamp();
      if( message.isImagePreview() ) preview = QStringLiteral( "[图片]" );
      else if( message.isVoice() ) preview = QStringLiteral( "[语音]" );
      else if( message.isFileTransfer() ) preview = QStringLiteral( "[文件] " ) + plain;
      else preview = plain;
      if( !preview.isEmpty() && !message.isFromSystem() )
      {
        if( message.isFromLocalUser() ) preview.prepend( QStringLiteral( "我：" ) );
        else if( c.isGroup() )
        {
          const User sender = UserManager::instance().findUser( message.userId() );
          if( sender.isValid() ) preview.prepend( sender.name() + QStringLiteral( "：" ) );
        }
      }
    }
    if( unreadRemaining > 0 && !message.isFromLocalUser() && !message.isFromSystem() )
    {
      mentioned = mentioned || shadowMentionsUser( plain, Settings::instance().localUser().name() );
      --unreadRemaining;
    }
    if( !preview.isEmpty() && unreadRemaining <= 0 ) break;
  }
  if( preview.isEmpty() ) preview = QStringLiteral( "暂无消息" );
  if( unsent_messages > 0 ) preview.prepend( QStringLiteral( "[待发送] " ) );
  setData( 0, ChatPreview, preview.left( 240 ) );
  setData( 0, ChatMentioned, mentioned );
  if( pending ) tool_tip += QStringLiteral( "\n待处理" );
  tool_tip += QStringLiteral( "\n" ) + preview.left( 240 );
  setText( 0, chat_name );
  setToolTip( 0, tool_tip );
  setData( 0, ChatUnreadMessages, c.unreadMessages() );
  setData( 0, ChatMessages, c.messages().size() + ChatManager::instance().savedChatSize( c.name() ) );
  setData( 0, ChatOnlineUsers, online_users );
  setData( 0, ChatLastMessageTimestamp, latestTimestamp );
  if( m_defaultIcon.isNull() )
    m_defaultIcon = IconManager::instance().icon( "chat.png" );

  setIcon( 0, m_defaultIcon );
  setForeground( 0, QBrush( QColor( "#243044" ) ) );
  QFont labelFont = ShadowStyle::applicationFont();
  labelFont.setBold( c.unreadMessages() > 0 && !muted );
  setFont( 0, labelFont );

  return true;
}
