#include "Updater.h"
#include "ShadowUpdateManifest.h"
#include "ShadowGitHubRelease.h"
#include "ShadowGitHubFallback.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QSslSocket>
#include <QTimer>

namespace
{
QString apiFailureText(int status, bool timeout, bool oversized,
  QNetworkReply::NetworkError error, const QString& error_string)
{
  if(timeout)
    return QStringLiteral("版本校验超时，请检查网络后重试。");
  if(oversized)
    return QStringLiteral("更新服务响应过大，无法校验版本。");
  if(error == QNetworkReply::SslHandshakeFailedError)
    return QStringLiteral("更新服务安全证书验证失败，已停止检查。");
  if(status == 403 || status == 429)
    return QStringLiteral("更新服务请求受限（限流），请稍后重试。");
  if(status >= 500)
    return QStringLiteral("更新服务暂时不可用（HTTP %1）。").arg(status);
  if(error != QNetworkReply::NoError)
    return QStringLiteral("无法连接更新服务（网络错误：%1）。").arg(error_string.left(300));
  if(status != 200)
    return QStringLiteral("更新服务返回异常状态：%1。").arg(status);
  return QStringLiteral("更新服务返回的内容不含本通道的有效版本。");
}
}

Updater::Updater(QObject* parent, const QUrl& manifest_url, const QString& test_channel)
  : QObject(parent), m_manifestUrl(manifest_url),
    m_channel(test_channel.isEmpty() || manifest_url.isEmpty() ?
              QString::fromLatin1(ShadowBuild::channel()) : test_channel)
{
  setObjectName("Updater");
}

QUrl Updater::productionManifestUrl()
{
#if defined(SHADOWCHAT_BUILD_MAIN)
  return QUrl(QStringLiteral("https://api.github.com/repos/shadowyli/shadow-chat/releases"));
#elif defined(SHADOWCHAT_LOCAL_UPDATE_PORT) && !defined(SHADOWCHAT_BUILD_DEV)
  return QUrl(QStringLiteral("http://127.0.0.1:%1/updates/shadowchat.json").arg(SHADOWCHAT_LOCAL_UPDATE_PORT));
#else
  return QUrl(QStringLiteral("https://api.github.com/repos/shadowyli/shadow-chat/releases"));
#endif
}

void Updater::setNetworkTimeoutsForTesting(int api_timeout_ms, int fallback_timeout_ms)
{
  m_apiTimeoutMs = qBound(50, api_timeout_ms, 60000);
  m_fallbackTimeoutMs = qBound(100, fallback_timeout_ms, 120000);
}

bool Updater::isApiBackedOff() const
{
  return m_apiBackoffUntil.isValid() && QDateTime::currentDateTimeUtc() < m_apiBackoffUntil;
}

bool Updater::hasNewerVersion(const QString& current) const
{
  return m_manifestValid && ShadowUpdateManifest::allowsVersion(current, current) &&
    ShadowUpdateManifest::allowsVersion(m_versionAvailable, current) &&
    QVersionNumber::compare(QVersionNumber::fromString(m_versionAvailable), QVersionNumber::fromString(current)) > 0;
}

void Updater::checkForNewVersion()
{
  if(m_checking)
    return;
  m_manifestValid = false;
  m_versionAvailable.clear();
  m_downloadUrl.clear();
  m_news.clear();
  m_error.clear();
  m_fallbackStarted = false;
  const QUrl url = m_manifestUrl.isEmpty() ? productionManifestUrl() : m_manifestUrl;
  const bool github = ShadowGitHubRelease::isSource(url);
  const bool local = (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https")) &&
    (url.host() == QLatin1String("localhost") || url.host() == QLatin1String("127.0.0.1") ||
     url.host() == QLatin1String("::1"));
  const bool githubResponse = github || (local && url.path() == QLatin1String("/repos/shadowyli/shadow-chat/releases"));
  if(!url.isValid() || (!m_manifestUrl.isEmpty() && !local) ||
     !url.userInfo().isEmpty() || url.hasQuery() || url.hasFragment() ||
     (!githubResponse && url.path() != QLatin1String("/updates/shadowchat.json")))
  {
    m_error = QStringLiteral("更新源无效：仅允许固定发布地址或测试构造器指定的本机地址。");
    emit jobCompleted();
    return;
  }
  if(url.scheme() == QLatin1String("https") && !QSslSocket::supportsSsl())
  {
    m_error = QStringLiteral("无法建立安全连接：缺少或不兼容的 TLS 运行库。请联系管理员修复安装后重试。");
    emit jobCompleted();
    return;
  }
  m_checking = true;
  const bool fallback_allowed = ShadowGitHubRelease::isApiSource(url);
  if(fallback_allowed && isApiBackedOff())
  {
    m_fallbackStarted = true;
    startFallback(QStringLiteral("更新服务请求受限（限流），已改用备用来源。"));
    return;
  }
  QNetworkAccessManager* manager = new QNetworkAccessManager(this);
  manager->setProxy(QNetworkProxy::NoProxy);
  QNetworkRequest request(url);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
  request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
  request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
  request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
  request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
  request.setRawHeader("Accept", githubResponse ? "application/vnd.github+json" : "application/json");
  request.setRawHeader("User-Agent", "ShadowChat");
  request.setRawHeader("Cache-Control", "no-cache, no-store");
  QNetworkReply* reply = manager->get(request);
  const qint64 maximumBytes = githubResponse ? 1024 * 1024 : 65536;
  reply->setReadBufferSize(maximumBytes + 1);
  QTimer* timeout = new QTimer(reply);
  timeout->setSingleShot(true);
  connect(timeout, &QTimer::timeout, reply, [reply]() {
    reply->setProperty("shadowTimedOut", true);
    reply->abort();
  });
  timeout->start(m_apiTimeoutMs);
  connect(reply, &QNetworkReply::readyRead, reply, [reply, maximumBytes]() {
    if(reply->bytesAvailable() > maximumBytes)
    {
      reply->setProperty("shadowOversized", true);
      reply->abort();
    }
  });
  connect(reply, &QNetworkReply::finished, this,
    [this, reply, manager, githubResponse, fallback_allowed, url]() {
      reply->deleteLater();
      manager->deleteLater();
      if(!m_checking || m_fallbackStarted)
        return;
      const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      const QNetworkReply::NetworkError networkError = reply->error();
      const bool timedOut = reply->property("shadowTimedOut").toBool();
      const bool oversized = reply->property("shadowOversized").toBool();
      applyApiBackoff(reply->rawHeader("Retry-After"), reply->rawHeader("X-RateLimit-Remaining"),
        reply->rawHeader("X-RateLimit-Reset"));
      if(!timedOut && !oversized && networkError == QNetworkReply::NoError && status == 200)
      {
        QByteArray data = reply->readAll();
        if(githubResponse)
          data = ShadowGitHubRelease::manifest(data, m_channel, QString::fromLatin1(ShadowBuild::environment()));
        if(ShadowUpdateManifest::parse(data, url, &m_versionAvailable, &m_downloadUrl, m_channel))
        {
          m_manifestValid = true;
          m_error.clear();
          if(!isApiBackedOff())
            m_apiBackoffUntil = QDateTime();
          readNews(data);
          finishCheck();
          return;
        }
      }
      const QString primary = apiFailureText(status, timedOut, oversized, networkError, reply->errorString());
      if(fallback_allowed && networkError != QNetworkReply::SslHandshakeFailedError && !oversized)
      {
        m_fallbackStarted = true;
        startFallback(primary);
        return;
      }
      m_error = primary;
      finishCheck();
    });
}

void Updater::applyApiBackoff(const QByteArray& retry_after, const QByteArray& remaining,
  const QByteArray& reset)
{
  const QDateTime now = QDateTime::currentDateTimeUtc();
  QDateTime until;
  bool ok = false;
  const qint64 seconds = retry_after.trimmed().toLongLong(&ok);
  if(ok && seconds > 0)
    until = now.addSecs(qMin<qint64>(seconds, 6 * 60 * 60));
  else if(!retry_after.trimmed().isEmpty())
  {
    const QDateTime parsed = QDateTime::fromString(QString::fromLatin1(retry_after.trimmed()),
      Qt::RFC2822Date);
    if(parsed.isValid())
      until = parsed.toUTC();
  }
  if(remaining.trimmed() == QByteArray("0"))
  {
    bool reset_ok = false;
    const qint64 epoch = reset.trimmed().toLongLong(&reset_ok);
    if(reset_ok && epoch > 0)
    {
      QDateTime reset_time = QDateTime::fromSecsSinceEpoch(epoch, Qt::UTC);
      if(reset_time > now && (!until.isValid() || reset_time > until))
        until = reset_time;
    }
  }
  if(until.isValid() && until > now)
  {
    const QDateTime limit = now.addSecs(6 * 60 * 60);
    m_apiBackoffUntil = until > limit ? limit : until;
  }
}

void Updater::startFallback(const QString& primary_error)
{
  const QUrl source = m_manifestUrl.isEmpty() ? productionManifestUrl() : m_manifestUrl;
  const QString environment = QString::fromLatin1(ShadowBuild::environment());
  m_fallback = new ShadowGitHubFallback(this, source, m_channel, environment,
    m_fallbackTimeoutMs, [this, primary_error, source](const QByteArray& manifest, const QString& error) {
      m_fallback = nullptr;
      QString fallback_error = error;
      if(fallback_error.isEmpty() && !manifest.isEmpty())
      {
        QString version;
        QString download;
        if(ShadowUpdateManifest::parse(manifest, source, &version, &download, m_channel))
        {
          m_manifestValid = true;
          m_versionAvailable = version;
          m_downloadUrl = download;
          m_error.clear();
          readNews(manifest);
        }
        else
          fallback_error = QStringLiteral("备用来源的安装包信息未通过校验。");
      }
      if(!m_manifestValid)
      {
        if(fallback_error.isEmpty())
          fallback_error = QStringLiteral("备用来源暂未提供有效版本。");
        m_error = primary_error.isEmpty() ? fallback_error :
          QStringLiteral("%1 %2").arg(primary_error, fallback_error);
      }
      m_fallbackStarted = false;
      finishCheck();
    });
  m_fallback->start();
}

void Updater::readNews(const QByteArray& manifest_data)
{
  m_news.clear();
  const QJsonValue notes = QJsonDocument::fromJson(manifest_data).object().value("notes");
  QStringList lines;
  if(notes.isArray())
  {
    for(const QJsonValue& note : notes.toArray())
      if(note.isString())
        lines.append(note.toString().left(500));
    m_news = lines.join("\n").left(2000);
  }
  else
    m_news = notes.toString().left(2000);
}

void Updater::finishCheck()
{
  m_checking = false;
  m_fallbackStarted = false;
  emit jobCompleted();
}

void Updater::onDownloadCompleted(const QString&) {}
