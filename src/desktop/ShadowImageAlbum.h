#ifndef SHADOWCHAT_IMAGEALBUM_H
#define SHADOWCHAT_IMAGEALBUM_H
#include "HistoryRecord.h"
#include <QJsonArray>

inline QStringList shadowAlbumOriginals(const QString& html)
{
  QStringList originals;
  foreach(const HistoryRecord& record,shadowChatHistoryRecords(html))foreach(const QString& address,record.media) {
    const QString path=QUrl(address).toLocalFile();
    if(!path.isEmpty()&&!originals.contains(path))originals<<path;
  }
  return originals;
}
inline QUrl shadowAlbumUrl(const QStringList& originals,int selected)
{
  QJsonArray paths;foreach(const QString& path,originals)paths.append(path);
  QUrl url;url.setScheme("shadowimage");url.setPath(QString::fromLatin1(QJsonDocument(paths).toJson(QJsonDocument::Compact).toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals)));url.setFragment(QString::number(selected));return url;
}
inline QStringList shadowAlbumPaths(const QUrl& url)
{
  if(url.scheme()!="shadowimage")return QStringList();
  const QJsonArray paths=QJsonDocument::fromJson(QByteArray::fromBase64(url.path().toLatin1(),QByteArray::Base64UrlEncoding)).array();
  QStringList result;foreach(const QJsonValue& value,paths)if(value.isString()&&QFileInfo(value.toString()).isAbsolute())result<<value.toString();return result;
}
#endif
