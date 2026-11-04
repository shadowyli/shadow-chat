#ifndef SHADOWGITHUBRELEASE_H
#define SHADOWGITHUBRELEASE_H

#include <QtCore>
#include <QCryptographicHash>
#include <QXmlStreamReader>
#include <algorithm>
#include <cmath>
#include "ShadowUpdateManifest.h"

namespace ShadowGitHubRelease {
struct FeedRelease
{
  QString version;
  QString tag;
  QString notes;
};

inline bool isSource(const QUrl& source)
{
  return source == QUrl(QStringLiteral("https://api.github.com/repos/shadowyli/shadow-chat/releases"));
}

inline bool isLoopbackHost(const QString& host)
{
  return host == QStringLiteral("localhost") || host == QStringLiteral("127.0.0.1") ||
    host == QStringLiteral("::1");
}

inline bool isLoopbackApi(const QUrl& source)
{
  return (source.scheme() == QStringLiteral("http") || source.scheme() == QStringLiteral("https")) &&
    isLoopbackHost(source.host()) && !source.hasQuery() && !source.hasFragment() &&
    source.userInfo().isEmpty() && source.path() == QStringLiteral("/repos/shadowyli/shadow-chat/releases");
}

inline bool isApiSource(const QUrl& source)
{
  return isSource(source) || isLoopbackApi(source);
}

inline QUrl feedUrl(const QUrl& source)
{
  if(isSource(source))
    return QUrl(QStringLiteral("https://github.com/shadowyli/shadow-chat/releases.atom"));
  if(!isLoopbackApi(source))
    return QUrl();
  QUrl url = source;
  url.setPath(QStringLiteral("/shadowyli/shadow-chat/releases.atom"));
  url.setQuery(QString());
  url.setFragment(QString());
  return url;
}

inline QUrl assetUrl(const QUrl& source, const QString& tag, const QString& name)
{
  static const QRegularExpression safeName(QStringLiteral("^[A-Za-z0-9_.-]+$"));
  static const QRegularExpression safeTag(QStringLiteral("^v[0-9]+\\.[0-9]+\\.[0-9]+(-test)?$"));
  if(!safeName.match(name).hasMatch() || !safeTag.match(tag).hasMatch())
    return QUrl();
  QUrl url;
  if(isSource(source))
  {
    url = QUrl(QStringLiteral("https://github.com"));
    url.setPath(QStringLiteral("/shadowyli/shadow-chat/releases/download/%1/%2").arg(tag, name));
  }
  else if(isLoopbackApi(source))
  {
    url = source;
    url.setPath(QStringLiteral("/downloads/%1/%2").arg(tag, name));
    url.setQuery(QString());
    url.setFragment(QString());
  }
  return url;
}

inline bool isTrustedAssetRedirect(const QUrl& destination)
{
  if(!destination.isValid() || destination.scheme() != QStringLiteral("https") ||
     destination.port(-1) != -1 || !destination.userInfo().isEmpty() ||
     destination.hasFragment() || destination.path().isEmpty())
    return false;
  return destination.host() == QStringLiteral("release-assets.githubusercontent.com") ||
    destination.host() == QStringLiteral("objects.githubusercontent.com");
}

inline QString decodeFeedNote(QString text)
{
  text.replace(QRegularExpression(QStringLiteral("<[^>]+>")), QStringLiteral(" "));
  text.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
  text.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
  text.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
  text.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
  text.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
  return text.simplified().left(300);
}

inline QList<FeedRelease> feedReleases(const QByteArray& data, const QString& channel)
{
  QList<FeedRelease> releases;
  if(data.isEmpty() || data.size() > 512 * 1024 || data.contains("<!DOCTYPE") ||
     (channel != QStringLiteral("test") && channel != QStringLiteral("stable")))
    return releases;
  const QRegularExpression tagPattern(channel == QStringLiteral("test") ?
    QStringLiteral("^v([0-9]+\\.[0-9]+\\.[0-9]+)-test$") :
    QStringLiteral("^v([0-9]+\\.[0-9]+\\.[0-9]+)$"));
  QXmlStreamReader xml(data);
  if(!xml.readNextStartElement() || xml.name() != QStringLiteral("feed") ||
     xml.namespaceUri() != QStringLiteral("http://www.w3.org/2005/Atom"))
    return releases;
  QSet<QString> tags;
  while(xml.readNextStartElement())
  {
    if(xml.name() != QStringLiteral("entry"))
    {
      xml.skipCurrentElement();
      continue;
    }
    QString id;
    QString link;
    QString content;
    int alternatives = 0;
    while(xml.readNextStartElement())
    {
      if(xml.name() == QStringLiteral("id"))
        id = xml.readElementText();
      else if(xml.name() == QStringLiteral("content"))
        content = xml.readElementText(QXmlStreamReader::SkipChildElements);
      else
      {
        if(xml.name() == QStringLiteral("link") &&
           xml.attributes().value(QStringLiteral("rel")) == QStringLiteral("alternate"))
        {
          link = xml.attributes().value(QStringLiteral("href")).toString();
          ++alternatives;
        }
        xml.skipCurrentElement();
      }
    }
    const QUrl address(link);
    const QString tag = address.path().section(QLatin1Char('/'), -1);
    const QRegularExpressionMatch match = tagPattern.match(tag);
    const QRegularExpression idPattern(QStringLiteral("^tag:github\\.com,2008:Repository/[0-9]+/%1$")
      .arg(QRegularExpression::escape(tag)));
    if(match.hasMatch() && address.host() == QStringLiteral("github.com") &&
       address.path() == QStringLiteral("/shadowyli/shadow-chat/releases/tag/%1").arg(tag) &&
       !ShadowUpdateManifest::allowsVersion(match.captured(1), match.captured(1)))
      return {};
    if(alternatives != 1 || !match.hasMatch() || tags.contains(tag) ||
       address.scheme() != QStringLiteral("https") || address.host() != QStringLiteral("github.com") ||
       address.port(-1) != -1 || !address.userInfo().isEmpty() ||
       address.hasQuery() || address.hasFragment() ||
       address.path() != QStringLiteral("/shadowyli/shadow-chat/releases/tag/%1").arg(tag) ||
       !idPattern.match(id).hasMatch() ||
       !ShadowUpdateManifest::allowsVersion(match.captured(1), match.captured(1)))
      continue;
    QStringList notes;
    const QRegularExpression items(QStringLiteral("<li\\b[^>]*>([\\s\\S]*?)</li>"),
      QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator iterator = items.globalMatch(content);
    while(iterator.hasNext() && notes.size() < 8)
      notes.append(decodeFeedNote(iterator.next().captured(1)));
    tags.insert(tag);
    releases.append(FeedRelease{match.captured(1), tag, notes.join(QLatin1Char('\n'))});
  }
  if(xml.hasError())
    return {};
  std::sort(releases.begin(), releases.end(), [](const FeedRelease& left, const FeedRelease& right) {
    return QVersionNumber::compare(QVersionNumber::fromString(left.version),
      QVersionNumber::fromString(right.version)) > 0;
  });
  if(releases.size() > 5)
    releases = releases.mid(0, 5);
  return releases;
}

enum class MetadataStatus { Invalid, NotApplicable, Valid };

inline bool validAssetSize(const QJsonValue& value)
{
  const double size = value.toDouble(-1);
  return value.isDouble() && std::isfinite(size) && size >= 1 &&
    size <= 160 * 1024 * 1024 && std::floor(size) == size;
}

inline bool validReleaseAsset(const QJsonObject& asset, const QString& tag, const QString& filename)
{
  const QUrl download(asset.value(QStringLiteral("browser_download_url")).toString());
  return asset.value(QStringLiteral("name")).toString() == filename &&
    validAssetSize(asset.value(QStringLiteral("size"))) &&
    QRegularExpression(QStringLiteral("^sha256:[a-fA-F0-9]{64}$"))
      .match(asset.value(QStringLiteral("digest")).toString()).hasMatch() &&
    download.isValid() && download.scheme() == QStringLiteral("https") &&
    download.host() == QStringLiteral("github.com") && download.port(-1) == -1 &&
    download.userInfo().isEmpty() && !download.hasQuery() && !download.hasFragment() &&
    download.path() == QStringLiteral("/shadowyli/shadow-chat/releases/download/%1/%2").arg(tag, filename);
}

inline QByteArray metadataManifest(const QByteArray& data, const FeedRelease& release,
  const QString& channel, const QString& environment, const QUrl& source,
  MetadataStatus* result = nullptr)
{
  if(result)
    *result = MetadataStatus::Invalid;
  if(data.isEmpty() || data.size() > 65536)
    return {};
  if((channel != QStringLiteral("test") && channel != QStringLiteral("stable")) ||
     (environment != QStringLiteral("local") && environment != QStringLiteral("dev") &&
      environment != QStringLiteral("main")))
    return {};
  QJsonParseError error;
  const QJsonDocument document = QJsonDocument::fromJson(data, &error);
  if(error.error != QJsonParseError::NoError || !document.isObject())
    return {};
  const QJsonObject metadata = document.object();
  const QJsonObject identity = metadata.value(QStringLiteral("binaryBuildIdentity")).toObject();
  const QJsonObject installer = metadata.value(QStringLiteral("installer")).toObject();
  const QString declaredEnvironment = metadata.value(QStringLiteral("releaseEnvironment")).toString();
  const QString declaredChannel = metadata.value(QStringLiteral("channel")).toString();
  const QString expectedTag = QStringLiteral("v%1%2").arg(release.version,
    declaredChannel == QStringLiteral("test") ? QStringLiteral("-test") : QString());
  const QString filename = QStringLiteral("ShadowChat-%1-%2-setup.exe").arg(release.version, declaredEnvironment);
  const QString digest = installer.value(QStringLiteral("sha256")).toString();
  const double size = installer.value(QStringLiteral("bytes")).toDouble(-1);
  if((declaredEnvironment != QStringLiteral("local") && declaredEnvironment != QStringLiteral("dev") &&
      declaredEnvironment != QStringLiteral("main")) ||
     (declaredChannel != QStringLiteral("test") && declaredChannel != QStringLiteral("stable")) ||
     !ShadowUpdateManifest::allowsVersion(release.version, release.version) || release.tag != expectedTag ||
     metadata.value(QStringLiteral("product")).toString().compare(QStringLiteral("ShadowChat"), Qt::CaseInsensitive) != 0 ||
     metadata.value(QStringLiteral("version")).toString() != release.version ||
     metadata.value(QStringLiteral("sourceTag")).toString() != release.tag ||
     identity.value(QStringLiteral("version")).toString() != release.version ||
     identity.value(QStringLiteral("channel")).toString() != declaredChannel ||
     identity.value(QStringLiteral("environment")).toString() != declaredEnvironment ||
     installer.value(QStringLiteral("file")).toString() != filename ||
     !QRegularExpression(QStringLiteral("^[a-fA-F0-9]{40}$"))
       .match(metadata.value(QStringLiteral("sourceCommit")).toString()).hasMatch() ||
     !QRegularExpression(QStringLiteral("^[a-fA-F0-9]{64}$")).match(digest).hasMatch() ||
     !validAssetSize(installer.value(QStringLiteral("bytes"))))
    return {};
  if(declaredChannel != channel || declaredEnvironment != environment)
  {
    if(result)
      *result = MetadataStatus::NotApplicable;
    return {};
  }
  const QUrl installerUrl = assetUrl(source, release.tag, filename);
  if(!installerUrl.isValid())
    return {};
  if(result)
    *result = MetadataStatus::Valid;
  QJsonArray notes;
  for(const QString& line : release.notes.split(QLatin1Char('\n'), Qt::SkipEmptyParts))
    notes.append(line);
  return QJsonDocument(QJsonObject{
    {QStringLiteral("schemaVersion"), 1}, {QStringLiteral("product"), QStringLiteral("shadowchat")},
    {QStringLiteral("version"), release.version}, {QStringLiteral("channel"), channel},
    {QStringLiteral("published"), true}, {QStringLiteral("mandatory"), false},
    {QStringLiteral("downloadPage"), QStringLiteral("https://github.com/shadowyli/shadow-chat/releases/tag/%1").arg(release.tag)},
    {QStringLiteral("installerUrl"), installerUrl.toString()},
    {QStringLiteral("sha256"), digest}, {QStringLiteral("size"), size}, {QStringLiteral("notes"), notes}
  }).toJson(QJsonDocument::Compact);
}

enum class ChecksumStatus
{
  Match,
  Missing,
  Mismatch,
  Malformed
};

inline ChecksumStatus checksums(const QByteArray& data, const QString& filename,
  const QString& sha256, const QByteArray& buildInfo)
{
  if(data.isEmpty() || data.size() > 8192)
    return ChecksumStatus::Malformed;
  const QRegularExpression linePattern(QStringLiteral("^([a-fA-F0-9]{64})[ \\t]+\\*?([^\\r\\n]+?)\\s*$"));
  const QString expectedBuildInfo = QString::fromLatin1(
    QCryptographicHash::hash(buildInfo, QCryptographicHash::Sha256).toHex());
  bool installerSeen = false;
  bool anyLine = false;
  for(const QString& raw : QString::fromLatin1(data).split(QLatin1Char('\n'), Qt::SkipEmptyParts))
  {
    const QRegularExpressionMatch match = linePattern.match(raw);
    if(!match.hasMatch())
      continue;
    anyLine = true;
    const QString digest = match.captured(1);
    const QString name = match.captured(2);
    if(name == filename)
    {
      installerSeen = true;
      if(digest.compare(sha256, Qt::CaseInsensitive) != 0)
        return ChecksumStatus::Mismatch;
    }
    else if(name == QStringLiteral("build-info.json"))
    {
      if(digest.compare(expectedBuildInfo, Qt::CaseInsensitive) != 0)
        return ChecksumStatus::Mismatch;
    }
  }
  if(!anyLine)
    return ChecksumStatus::Malformed;
  return installerSeen ? ChecksumStatus::Match : ChecksumStatus::Missing;
}

inline QByteArray manifest(const QByteArray& data, const QString& channel, const QString& environment)
{
  if(data.size() > 1024 * 1024) return QByteArray();
  QJsonParseError error;
  const QJsonDocument document = QJsonDocument::fromJson(data, &error);
  if(error.error != QJsonParseError::NoError || !document.isArray() ||
     (channel != QStringLiteral("test") && channel != QStringLiteral("stable")) ||
     (environment != QStringLiteral("local") && environment != QStringLiteral("dev") && environment != QStringLiteral("main")))
    return QByteArray();
  QJsonObject selected;
  QVersionNumber selectedVersion;
  QVersionNumber uncertainVersion;
  for(const QJsonValue& value : document.array())
  {
    const QJsonObject release = value.toObject();
    if(release.value(QStringLiteral("draft")).toBool())
      continue;
    const QString tag = release.value(QStringLiteral("tag_name")).toString();
    const QRegularExpression pattern(channel == QStringLiteral("test") ?
      QStringLiteral("^v([0-9]+\\.[0-9]+\\.[0-9]+)-test$") : QStringLiteral("^v([0-9]+\\.[0-9]+\\.[0-9]+)$"));
    const QRegularExpressionMatch match = pattern.match(tag);
    if(!match.hasMatch()) continue;
    const QString version = match.captured(1);
    if(!ShadowUpdateManifest::allowsVersion(version, version)) return {};
    const QVersionNumber number = QVersionNumber::fromString(version);
    if(!selected.isEmpty() && QVersionNumber::compare(number, selectedVersion) <= 0) continue;
    if(!release.value(QStringLiteral("draft")).isBool() ||
       !release.value(QStringLiteral("prerelease")).isBool() ||
       release.value(QStringLiteral("prerelease")).toBool() != (channel == QStringLiteral("test")))
    {
      if(QVersionNumber::compare(number, uncertainVersion) > 0)
        uncertainVersion = number;
      continue;
    }
    const QString filename = QStringLiteral("ShadowChat-%1-%2-setup.exe").arg(version, environment);
    bool applicable = false;
    bool anotherEnvironment = false;
    bool accepted = false;
    for(const QJsonValue& assetValue : release.value(QStringLiteral("assets")).toArray())
    {
      const QJsonObject asset = assetValue.toObject();
      const QString name = asset.value(QStringLiteral("name")).toString();
      if(name != filename)
      {
        if(QRegularExpression(QStringLiteral("^ShadowChat-%1-(local|dev|main)-setup\\.exe$")
            .arg(QRegularExpression::escape(version))).match(name).hasMatch() &&
           validReleaseAsset(asset, tag, name))
          anotherEnvironment = true;
        continue;
      }
      applicable = true;
      const QString digest = asset.value(QStringLiteral("digest")).toString();
      if(!validReleaseAsset(asset, tag, filename)) continue;
      const QUrl download(asset.value(QStringLiteral("browser_download_url")).toString());
      QJsonArray notes;
      for(const QString& line : release.value(QStringLiteral("body")).toString().split(QLatin1Char('\n')))
        if(line.startsWith(QStringLiteral("- ")) && notes.size() < 8) notes.append(line.mid(2).left(300));
      selected = QJsonObject{
        {QStringLiteral("schemaVersion"), 1}, {QStringLiteral("product"), QStringLiteral("shadowchat")},
        {QStringLiteral("version"), version}, {QStringLiteral("channel"), channel},
        {QStringLiteral("published"), true},
        {QStringLiteral("downloadPage"), QStringLiteral("https://github.com/shadowyli/shadow-chat/releases/tag/%1").arg(tag)},
        {QStringLiteral("installerUrl"), download.toString()}, {QStringLiteral("sha256"), digest.mid(7)},
        {QStringLiteral("size"), asset.value(QStringLiteral("size"))},
        {QStringLiteral("notes"), notes}, {QStringLiteral("mandatory"), false}
      };
      selectedVersion = number;
      accepted = true;
      break;
    }
    if(!accepted && (applicable || !anotherEnvironment) &&
       QVersionNumber::compare(number, uncertainVersion) > 0)
      uncertainVersion = number;
  }
  if(QVersionNumber::compare(uncertainVersion, selectedVersion) > 0)
    return {};
  return selected.isEmpty() ? QByteArray() : QJsonDocument(selected).toJson(QJsonDocument::Compact);
}
}

#endif
