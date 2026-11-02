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
// $Id: IconManager.cpp 1455 2020-12-23 10:17:53Z mastroddi $
//
//////////////////////////////////////////////////////////////////////

#include "IconManager.h"
#include <QHash>


IconManager* IconManager::mp_instance = Q_NULLPTR;


IconManager::IconManager()
 : m_defaultSourcePath( ":/images" ), m_sourcePath( "" )
{
}

QString IconManager::iconPath( const QString& icon_name ) const
{
  static const QHash<QString, QString> modern_icons = {
    { "settings.png", "gear-six" }, { "chat-settings.png", "gear-six" },
    { "chat.png", "chats-circle" }, { "chat-list.png", "chats-circle" },
    { "message-create.png", "chats-circle" }, { "unsent-message.png", "chats-circle" },
    { "search.png", "magnifying-glass" }, { "find.png", "magnifying-glass" },
    { "send-file.png", "paperclip" }, { "file-transfer.png", "paperclip" },
    { "file-transfer-progress.png", "paperclip" }, { "send-folder.png", "folder-open" },
    { "group.png", "users-three" }, { "group-list.png", "users-three" },
    { "group-create.png", "users-three" }, { "group-edit.png", "users-three" },
    { "user-list.png", "user-circle" }, { "profile-edit.png", "user-circle" },
    { "vcard.png", "user-circle" }, { "user.png", "user-circle" },
    { "folder.png", "folder-open" }, { "folder-open.png", "folder-open" },
    { "saved-chat.png", "archive" }, { "save.png", "archive" },
    { "history.png", "archive" }, { "log.png", "archive" },
    { "emoticon.png", "smiley" }, { "send.png", "paper-plane-tilt" },
    { "clear.png", "x" }, { "delete.png", "x" },
    { "remove.png", "x" }, { "close.png", "x" }
  };
  const auto modern = modern_icons.constFind( icon_name );
  if( modern != modern_icons.constEnd() )
    return QStringLiteral( ":/images/shadowchat/icons/%1.svg" ).arg( modern.value() );
  if( hasDefaultIcons() )
    return defaulIconPath( icon_name );

  QString custom_icon_path = customIconPath( icon_name );
  if( QFile::exists( custom_icon_path ) )
    return custom_icon_path;
  else
    return defaulIconPath( icon_name );
}
