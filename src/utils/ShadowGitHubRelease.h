#ifndef SHADOWGITHUBRELEASE_H
#define SHADOWGITHUBRELEASE_H

#include <QtCore>

namespace ShadowGitHubRelease {
inline bool isSource(const QUrl& source)
{
  return source == QUrl(QStringLiteral("https://api.github.com/repos/shadowyli/shadow-chat/releases"));
}

inline QByteArray manifest(const QByteArray& data, const QString& channel, const QString& environment)
{
  QJsonParseError error;
  const QJsonDocument document = QJsonDocument::fromJson(data, &error);
  if(error.error != QJsonParseError::NoError || !document.isArray() ||
     (channel != QStringLiteral("test") && channel != QStringLiteral("stable")) ||
     (environment != QStringLiteral("local") && environment != QStringLiteral("dev") && environment != QStringLiteral("main")))
    return QByteArray();
  QJsonObject selected;
  QVersionNumber selectedVersion;
  for(const QJsonValue& value : document.array())
  {
    const QJsonObject release = value.toObject();
    if(release.value("draft").toBool() || !release.contains("prerelease") ||
       release.value("prerelease").toBool() != (channel == QStringLiteral("test")))
      continue;
    const QString tag = release.value("tag_name").toString();
    const QRegularExpression pattern(channel == QStringLiteral("test") ?
      QStringLiteral("^v([0-9]+\\.[0-9]+\\.[0-9]+)-test$") : QStringLiteral("^v([0-9]+\\.[0-9]+\\.[0-9]+)$"));
    const QRegularExpressionMatch match = pattern.match(tag);
    if(!match.hasMatch()) continue;
    const QString version = match.captured(1);
    const QVersionNumber number = QVersionNumber::fromString(version);
    if(!selected.isEmpty() && QVersionNumber::compare(number, selectedVersion) <= 0) continue;
    const QString filename = QStringLiteral("ShadowChat-%1-%2-setup.exe").arg(version, environment);
    const QString legacyFilename = QStringLiteral("ShadowChat-%1-setup.exe").arg(version);
    for(const QJsonValue& assetValue : release.value("assets").toArray())
    {
      const QJsonObject asset = assetValue.toObject();
      const QString name = asset.value("name").toString();
      if(name != filename && !(environment == QStringLiteral("main") && name == legacyFilename)) continue;
      const QString digest = asset.value("digest").toString();
      if(!QRegularExpression(QStringLiteral("^sha256:[a-fA-F0-9]{64}$")).match(digest).hasMatch() ||
         asset.value("size").toDouble() <= 0) continue;
      const QUrl download(asset.value("browser_download_url").toString());
      if(download.scheme() != QStringLiteral("https") || download.host() != QStringLiteral("github.com") ||
         download.port(-1) != -1 || !download.userInfo().isEmpty() || download.hasQuery() || download.hasFragment() ||
         download.path() != QStringLiteral("/shadowyli/shadow-chat/releases/download/%1/%2").arg(tag, name))
        continue;
      QJsonArray notes;
      for(const QString& line : release.value("body").toString().split('\n'))
        if(line.startsWith(QStringLiteral("- ")) && notes.size() < 8) notes.append(line.mid(2).left(300));
      selected = QJsonObject{
        {"schemaVersion", 1}, {"product", "shadowchat"}, {"version", version}, {"channel", channel},
        {"published", true}, {"downloadPage", QStringLiteral("https://github.com/shadowyli/shadow-chat/releases/tag/%1").arg(tag)},
        {"installerUrl", download.toString()}, {"sha256", digest.mid(7)}, {"size", asset.value("size")},
        {"notes", notes}, {"mandatory", false}
      };
      selectedVersion = number;
      break;
    }
  }
  return selected.isEmpty() ? QByteArray() : QJsonDocument(selected).toJson(QJsonDocument::Compact);
}
}

#endif
