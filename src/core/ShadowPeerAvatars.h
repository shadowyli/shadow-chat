#ifndef SHADOW_PEER_AVATARS_H
#define SHADOW_PEER_AVATARS_H
#include "Settings.h"
#include "User.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QImageReader>
#include <QSettings>

namespace ShadowPeerAvatars {
inline QString path() { return QDir(Settings::instance().dataFolder()).filePath("peer-avatars.ini"); }
inline QString key(const User& user) {
  const QString identity = user.hash().isEmpty() ? "account:" + user.accountPath().trimmed().toLower() : "hash:" + user.hash();
  return QString::fromLatin1(QCryptographicHash::hash(identity.toUtf8(),QCryptographicHash::Sha256).toHex());
}
inline QByteArray encode(const QPixmap& photo) {
  QByteArray png; QBuffer buffer(&png); buffer.open(QIODevice::WriteOnly);
  photo.scaled(QSize(512,512),Qt::KeepAspectRatio,Qt::SmoothTransformation).save(&buffer,"PNG");
  return png.toBase64();
}
inline QPixmap decode(const QByteArray& encoded) {
  if(encoded.isEmpty() || encoded.size()>2*1024*1024) return QPixmap();
  QByteArray png=QByteArray::fromBase64(encoded); QBuffer buffer(&png); buffer.open(QIODevice::ReadOnly);
  QImageReader reader(&buffer,"PNG"); const QSize size=reader.size();
  if(!size.isValid() || size.width()>2048 || size.height()>2048) return QPixmap();
  return QPixmap::fromImage(reader.read());
}
inline void restore(User* user) {
  if(user->isLocal() || QFileInfo(path()).size()>16*1024*1024) return;
  QSettings store(path(),QSettings::IniFormat); const QPixmap photo=decode(store.value(key(*user)).toByteArray());
  if(!photo.isNull()) { VCard card=user->vCard(); card.setPhoto(photo); user->setVCard(card); }
}
inline void save(const QList<User>& users) {
  QSettings store(path(),QSettings::IniFormat); store.clear();
  foreach(const User& user,users) if(!user.isLocal() && !user.vCard().photo().isNull()) store.setValue(key(user),encode(user.vCard().photo()));
  store.sync();
  if(store.status()!=QSettings::NoError) qWarning()<<"Unable to persist peer avatars";
}
}
#endif
