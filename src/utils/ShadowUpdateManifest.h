#ifndef SHADOWUPDATEMANIFEST_H
#define SHADOWUPDATEMANIFEST_H
#include <QtCore>
#include "../core/ShadowBuild.h"
namespace ShadowUpdateManifest {
inline bool allowsVersion(const QString& current, const QString& released) {
  const QRegularExpression format(QStringLiteral("^[0-9]+\\.[0-9]+\\.[0-9]+$"));
  if(!format.match(current).hasMatch() || !format.match(released).hasMatch()) return false;
  for(const QString& number : {current, released})
    for(const QString& segment : number.split('.')) {
      bool valid = false;
      const qulonglong value = segment.toULongLong(&valid);
      if(!valid || value > 2147483647ULL) return false;
    }
  int currentEnd = 0, releasedEnd = 0;
  const QVersionNumber currentNumber = QVersionNumber::fromString(current, &currentEnd);
  const QVersionNumber releasedNumber = QVersionNumber::fromString(released, &releasedEnd);
  return currentEnd == current.size() && releasedEnd == released.size() &&
    !currentNumber.isNull() && !releasedNumber.isNull() &&
    QVersionNumber::compare(currentNumber, releasedNumber) >= 0;
}
inline bool parse(const QByteArray& data, const QUrl& source, QString* version, QString* download,
                  const QString& channel = QString::fromLatin1(ShadowBuild::channel())) {
  version->clear(); download->clear();
  if(data.size()>65536) return false;
  QJsonParseError error;
  const QJsonDocument document=QJsonDocument::fromJson(data,&error);
  if(error.error!=QJsonParseError::NoError || !document.isObject()) return false;
  const QJsonObject object=document.object();
  if(object.value("schemaVersion").toInt()!=1 || object.value("product").toString()!=QStringLiteral("shadowchat") || !object.value("published").toBool()) return false;
  if((channel != QStringLiteral("test") && channel != QStringLiteral("stable")) ||
     object.value("channel").toString() != channel) return false;
  const QString number=object.value("version").toString();
  if(!allowsVersion(number, number)) return false;
  const QString pageText = object.value("downloadPage").toString();
  if(pageText.isEmpty()) return false;
  const QUrl page=source.resolved(QUrl(pageText));
  if(page.scheme()!=source.scheme() || page.host()!=source.host() || page.port()!=source.port() ||
     !page.userInfo().isEmpty() || page.hasQuery() || page.hasFragment() ||
     page.path()!=QStringLiteral("/tools/shadowchat") || object.value("installerUrl").toString().isEmpty() ||
     !QRegularExpression(QStringLiteral("^[a-fA-F0-9]{64}$")).match(object.value("sha256").toString()).hasMatch()) return false;
  *version=number; *download=page.toString(); return true;
}
}
#endif
