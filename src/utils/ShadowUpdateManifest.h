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
  const QString installerText = object.value("installerUrl").toString();
  if(installerText.isEmpty()) return false;
  const QUrl installer=source.resolved(QUrl(installerText));
  const QString tag = QStringLiteral("v%1%2").arg(number,
    channel == QStringLiteral("test") ? QStringLiteral("-test") : QString());
  const QString filename = installer.fileName();
  const QRegularExpression assetName(QStringLiteral("^ShadowChat-%1-(local|dev|main)-setup\\.exe$")
    .arg(QRegularExpression::escape(number)));
  const bool github = installer.scheme() == QStringLiteral("https") && installer.host() == QStringLiteral("github.com") &&
    installer.port(-1) == -1 && (assetName.match(filename).hasMatch() ||
      (channel == QStringLiteral("stable") && filename == QStringLiteral("ShadowChat-%1-setup.exe").arg(number))) &&
    installer.path() == QStringLiteral("/shadowyli/shadow-chat/releases/download/%1/%2").arg(tag, filename);
  const bool mirror = source.host() != QStringLiteral("api.github.com") &&
    (source.scheme() == QStringLiteral("http") || source.scheme() == QStringLiteral("https")) &&
    installer.scheme() == source.scheme() && installer.host() == source.host() && installer.port() == source.port() &&
    QRegularExpression(QStringLiteral("^/downloads/[A-Za-z0-9_./-]+\\.exe$")).match(installer.path()).hasMatch() &&
    !installer.path().contains(QStringLiteral(".."));
  if((!github && !mirror) || !installer.isValid() ||
     !installer.userInfo().isEmpty() || installer.hasQuery() || installer.hasFragment() ||
     !QRegularExpression(QStringLiteral("^[a-fA-F0-9]{64}$")).match(object.value("sha256").toString()).hasMatch()) return false;
  *version=number; *download=installer.toString(); return true;
}
}
#endif
