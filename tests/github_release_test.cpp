#include "ShadowGitHubRelease.h"
#include "ShadowUpdateManifest.h"
#include <cstdio>

#define CHECK(value) do { if(!(value)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #value); return 1; } } while(false)

static QJsonObject release(const QString& version, const QString& environment, bool prerelease = true)
{
  const QString tag = "v" + version + (prerelease ? "-test" : "");
  const QString filename = QStringLiteral("ShadowChat-%1-%2-setup.exe").arg(version, environment);
  return QJsonObject{
    {"tag_name", tag}, {"prerelease", prerelease}, {"draft", false}, {"body", "- Native UI improvements\n- Sending feedback"},
    {"assets", QJsonArray{QJsonObject{
      {"name", filename}, {"size", 1000}, {"digest", "sha256:" + QString(64, 'a')},
      {"browser_download_url", QStringLiteral("https://github.com/shadowyli/shadow-chat/releases/download/%1/%2").arg(tag, filename)}
    }}}
  };
}

int main(int argc, char** argv)
{
  QCoreApplication app(argc, argv);
  const QUrl source("https://api.github.com/repos/shadowyli/shadow-chat/releases");
  CHECK(ShadowGitHubRelease::isSource(source));
  CHECK(!ShadowGitHubRelease::isSource(QUrl("https://api.github.com/repos/other/project/releases")));
  CHECK(!ShadowGitHubRelease::isSource(QUrl(source.toString() + "?redirect=other")));
  auto parse = [](const QJsonArray& releases, const QString& channel = "test", const QString& environment = "dev") {
    return ShadowGitHubRelease::manifest(QJsonDocument(releases).toJson(), channel, environment);
  };
  const QJsonObject older = release("1.1.2", "dev");
  const QJsonObject latest = release("1.1.3", "dev");
  const QByteArray converted = parse(QJsonArray{latest, older, release("9.0.0", "local")});
  QString version, download;
  CHECK(ShadowUpdateManifest::parse(converted, source, &version, &download, "test"));
  CHECK(version == "1.1.3");
  CHECK(download == "https://github.com/shadowyli/shadow-chat/releases/tag/v1.1.3-test");
  CHECK(QJsonDocument::fromJson(converted).object().value("notes").toArray().size() == 2);
  CHECK(!QJsonDocument::fromJson(converted).object().value("mandatory").toBool());
  CHECK(parse(QJsonArray{latest}, "stable", "main").isEmpty());
  CHECK(!parse(QJsonArray{release("1.1.3", "main", false)}, "stable", "main").isEmpty());
  CHECK(parse(QJsonArray{latest}, "test", "local").isEmpty());
  QJsonObject draft = latest;
  draft["draft"] = true;
  CHECK(parse(QJsonArray{draft}).isEmpty());
  QJsonObject invalid = latest;
  QJsonObject asset = latest.value("assets").toArray().first().toObject();
  asset["browser_download_url"] = "https://github.com/other/project/releases/download/v1.1.3-test/ShadowChat-1.1.3-dev-setup.exe";
  invalid["assets"] = QJsonArray{asset};
  CHECK(parse(QJsonArray{invalid}).isEmpty());
  asset = latest.value("assets").toArray().first().toObject();
  asset["digest"] = QJsonValue();
  invalid["assets"] = QJsonArray{asset};
  CHECK(parse(QJsonArray{invalid}).isEmpty());
  QJsonObject unsafe = QJsonDocument::fromJson(converted).object();
  unsafe["downloadPage"] = "https://github.com/other/project/releases/tag/v1.1.3-test";
  CHECK(!ShadowUpdateManifest::parse(QJsonDocument(unsafe).toJson(), source, &version, &download, "test"));
  CHECK(ShadowGitHubRelease::manifest("{}", "test", "dev").isEmpty());
  std::puts("PASS personal repository releases: version order, channels, environment, drafts, digest, notes and repository URL restrictions");
  return 0;
}
