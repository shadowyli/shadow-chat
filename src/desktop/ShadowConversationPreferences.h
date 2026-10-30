#ifndef SHADOWCONVERSATIONPREFERENCES_H
#define SHADOWCONVERSATIONPREFERENCES_H
#include "Settings.h"
#include "Chat.h"
#include "UserManager.h"

// Display preferences only: never deletes history, files, members or queued sends.
class ShadowConversationPreferences
{
public:
  explicit ShadowConversationPreferences(const QString& root=QString()) : m_root(root.isEmpty()?Settings::instance().dataFolder():root) {}
  static QString fileName(){return QStringLiteral("conversation-preferences.ini");}
  static QString key(const Chat& chat)
  {
    if(chat.isDefault())return QStringLiteral("default");
    if(chat.isGroup())return chat.privateId().isEmpty()?QString():QStringLiteral("group:")+chat.privateId();
    if(chat.isPrivate()) {
      const User user=UserManager::instance().findUser(chat.privateUserId());
      if(!user.hash().isEmpty())return QStringLiteral("peer:")+user.hash();
      if(!user.accountName().isEmpty())return QStringLiteral("account:")+user.accountPath().toLower();
    }
    return QString();
  }
  bool pinned(const Chat& chat) const{return read(chat,"pinned");}
  bool muted(const Chat& chat) const{return read(chat,"muted");}
  bool hidden(const Chat& chat) const{return read(chat,"hidden");}
  bool pending(const Chat& chat) const{return read(chat,"pending");}
  bool setPinned(const Chat& chat,bool value) const{return write(chat,"pinned",value);}
  bool setMuted(const Chat& chat,bool value) const{return write(chat,"muted",value);}
  bool setHidden(const Chat& chat,bool value) const{return write(chat,"hidden",value);}
  bool setPending(const Chat& chat,bool value) const{return write(chat,"pending",value);}
  bool reveal(const Chat& chat) const{return !hidden(chat) || setHidden(chat,false);}
private:
  QString setting(const Chat& chat,const QString& name) const
  {
    const QString identity=key(chat);
    return identity.isEmpty()?QString():QString::fromLatin1(QCryptographicHash::hash(identity.toUtf8(),QCryptographicHash::Sha256).toHex())+QLatin1Char('/')+name;
  }
  bool read(const Chat& chat,const QString& name) const
  {
    const QString entry=setting(chat,name);if(entry.isEmpty())return false;
    QSettings settings(QDir(m_root).filePath(fileName()),QSettings::IniFormat);return settings.value(entry,false).toBool();
  }
  bool write(const Chat& chat,const QString& name,bool value) const
  {
    const QString entry=setting(chat,name);if(entry.isEmpty() || !QDir().mkpath(m_root))return false;
    QSettings settings(QDir(m_root).filePath(fileName()),QSettings::IniFormat);settings.setValue(entry,value);settings.sync();return settings.status()==QSettings::NoError;
  }
  QString m_root;
};
#endif
