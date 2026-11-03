#ifndef SHADOWGITHUBFALLBACK_H
#define SHADOWGITHUBFALLBACK_H

#include "ShadowGitHubRelease.h"
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QTimer>
#include <functional>

class ShadowGitHubFallback final : public QObject
{
public:
  using Completion = std::function<void(const QByteArray& manifest, const QString& error)>;

  ShadowGitHubFallback(QObject* parent, const QUrl& apiSource, const QString& channel,
    const QString& environment, int totalTimeoutMs, Completion completion)
    : QObject(parent), m_source(apiSource), m_channel(channel), m_environment(environment),
      m_completion(std::move(completion)), m_manager(this), m_deadline(this),
      m_requestTimeoutMs(qBound(500, totalTimeoutMs / 3, 8000)), m_totalTimeoutMs(totalTimeoutMs)
  {
    m_manager.setProxy(QNetworkProxy::NoProxy);
    m_deadline.setSingleShot(true);
    connect(&m_deadline, &QTimer::timeout, this, [this]() {
      finish({}, QStringLiteral("备用版本查询超时，请稍后重试。"));
    });
  }

  void start()
  {
    m_deadline.start(m_totalTimeoutMs);
    const QUrl feed = ShadowGitHubRelease::feedUrl(m_source);
    if(!feed.isValid())
    {
      finish({}, QStringLiteral("备用版本来源地址无效。"));
      return;
    }
    fetch(feed, Kind::Feed, 0);
  }

private:
  enum class Kind { Feed, Metadata, Checksums };

  void finish(const QByteArray& manifest, const QString& error)
  {
    if(m_finished)
      return;
    m_finished = true;
    m_deadline.stop();
    if(m_reply && m_reply->isRunning())
      m_reply->abort();
    const Completion completion = std::move(m_completion);
    deleteLater();
    if(completion)
      completion(manifest, error);
  }

  void nextCandidate()
  {
    if(m_finished)
      return;
    while(++m_candidate < m_releases.size())
    {
      const ShadowGitHubRelease::FeedRelease& release = m_releases.at(m_candidate);
      const QUrl metadata = ShadowGitHubRelease::assetUrl(m_source, release.tag,
        QStringLiteral("build-info.json"));
      if(!metadata.isValid())
        continue;
      fetch(metadata, Kind::Metadata, 0);
      return;
    }
    finish({}, QStringLiteral("备用来源暂未提供本通道、环境的有效安装包信息。"));
  }

  void fetch(const QUrl& url, Kind kind, int redirects)
  {
    if(m_finished)
      return;
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    request.setRawHeader("Accept", kind == Kind::Feed ? "application/atom+xml" : "application/octet-stream");
    request.setRawHeader("User-Agent", "ShadowChat");
    QNetworkReply* reply = m_manager.get(request);
    m_reply = reply;
    const qint64 maximumBytes = kind == Kind::Feed ? 512 * 1024 :
      (kind == Kind::Checksums ? 8192 : 65536);
    reply->setReadBufferSize(maximumBytes + 1);
    QTimer* timeout = new QTimer(reply);
    timeout->setSingleShot(true);
    connect(timeout, &QTimer::timeout, reply, [reply]() {
      reply->setProperty("shadowFallbackTimedOut", true);
      reply->abort();
    });
    timeout->start(m_requestTimeoutMs);
    connect(reply, &QNetworkReply::readyRead, reply, [reply, maximumBytes]() {
      if(reply->bytesAvailable() > maximumBytes)
      {
        reply->setProperty("shadowFallbackOversized", true);
        reply->abort();
      }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, kind, redirects]() {
      reply->deleteLater();
      if(m_finished)
        return;
      const QUrl requestUrl = reply->url();
      const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      const QNetworkReply::NetworkError networkError = reply->error();
      if(networkError == QNetworkReply::SslHandshakeFailedError)
      {
        finish({}, QStringLiteral("备用来源安全证书验证失败，已停止检查。"));
        return;
      }
      if(reply->property("shadowFallbackTimedOut").toBool())
      {
        finish({}, kind == Kind::Feed ? QStringLiteral("备用版本列表查询超时，请稍后重试。") :
          QStringLiteral("备用版本信息查询超时，请稍后重试。"));
        return;
      }
      if(reply->property("shadowFallbackOversized").toBool())
      {
        finish({}, QStringLiteral("备用来源响应过大，已拒绝校验。"));
        return;
      }
      if(status == 301 || status == 302 || status == 303 || status == 307 || status == 308)
      {
        const QUrl target = requestUrl.resolved(
          reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl());
        if(kind == Kind::Feed || redirects >= 3 || !ShadowGitHubRelease::isTrustedAssetRedirect(target))
        {
          finish({}, QStringLiteral("备用来源跳转地址不受信任，已拒绝。"));
          return;
        }
        fetch(target, kind, redirects + 1);
        return;
      }
      if(status == 404 || status == 410)
      {
        if(kind == Kind::Feed)
          finish({}, QStringLiteral("备用版本列表不可用，请稍后重试。"));
        else
          finish({}, QStringLiteral("备用发布资产缺失，无法确认本通道的最新版本。"));
        return;
      }
      if(networkError != QNetworkReply::NoError)
      {
        finish({}, QStringLiteral("备用版本查询网络错误，请检查网络后重试。"));
        return;
      }
      if(status != 200)
      {
        finish({}, QStringLiteral("备用来源返回异常状态：%1。").arg(status));
        return;
      }
      const QByteArray data = reply->readAll();
      if(kind == Kind::Feed)
      {
        m_releases = ShadowGitHubRelease::feedReleases(data, m_channel);
        if(m_releases.isEmpty())
        {
          finish({}, QStringLiteral("备用版本列表不含本通道的有效发布记录。"));
          return;
        }
        nextCandidate();
        return;
      }
      if(kind == Kind::Metadata)
      {
        ShadowGitHubRelease::MetadataStatus result;
        const QByteArray manifest = ShadowGitHubRelease::metadataManifest(
          data, m_releases.at(m_candidate), m_channel, m_environment, m_source, &result);
        if(result == ShadowGitHubRelease::MetadataStatus::NotApplicable)
        {
          nextCandidate();
          return;
        }
        if(manifest.isEmpty())
        {
          finish({}, QStringLiteral("备用发布元信息未通过校验，无法确认最新版本。"));
          return;
        }
        const QJsonObject manifestObject = QJsonDocument::fromJson(manifest).object();
        m_metadata = data;
        m_manifest = manifest;
        m_installerName.clear();
        m_installerSha.clear();
        const QString urlText = manifestObject.value(QStringLiteral("installerUrl")).toString();
        m_installerName = QUrl(urlText).fileName();
        m_installerSha = manifestObject.value(QStringLiteral("sha256")).toString();
        const QUrl checksums = ShadowGitHubRelease::assetUrl(m_source,
          m_releases.at(m_candidate).tag, QStringLiteral("SHA256SUMS.txt"));
        if(!checksums.isValid() || m_installerName.isEmpty() || m_installerSha.size() != 64)
        {
          finish({}, QStringLiteral("备用安装包信息不完整，无法确认最新版本。"));
          return;
        }
        fetch(checksums, Kind::Checksums, 0);
        return;
      }
      const ShadowGitHubRelease::ChecksumStatus checksum = ShadowGitHubRelease::checksums(
        data, m_installerName, m_installerSha, m_metadata);
      if(checksum == ShadowGitHubRelease::ChecksumStatus::Mismatch)
      {
        finish({}, QStringLiteral("备用安装包校验信息不一致，已拒绝本次更新。"));
        return;
      }
      if(checksum != ShadowGitHubRelease::ChecksumStatus::Match)
      {
        finish({}, QStringLiteral("备用安装包校验信息缺失或无效，无法确认最新版本。"));
        return;
      }
      finish(m_manifest, QString());
    });
  }

  QUrl m_source;
  QString m_channel;
  QString m_environment;
  Completion m_completion;
  QNetworkAccessManager m_manager;
  QTimer m_deadline;
  QPointer<QNetworkReply> m_reply;
  QList<ShadowGitHubRelease::FeedRelease> m_releases;
  QByteArray m_metadata;
  QByteArray m_manifest;
  QString m_installerName;
  QString m_installerSha;
  int m_requestTimeoutMs;
  int m_totalTimeoutMs;
  int m_candidate = -1;
  bool m_finished = false;
};

#endif
