#include "Updater.h"
#include "ShadowUpdateManifest.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QSslSocket>
#include <QTimer>

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
  return QUrl(QStringLiteral("https://flowx.gz/updates/shadowchat.json"));
#elif defined(SHADOWCHAT_LOCAL_UPDATE_PORT) && !defined(SHADOWCHAT_BUILD_DEV)
  return QUrl(QStringLiteral("http://127.0.0.1:%1/updates/shadowchat.json").arg(SHADOWCHAT_LOCAL_UPDATE_PORT));
#else
  return QUrl(QStringLiteral("https://dev.flowx.gz/updates/shadowchat.json"));
#endif
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
  const QUrl url = m_manifestUrl.isEmpty() ? productionManifestUrl() : m_manifestUrl;
  const bool local = (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https")) &&
    (url.host() == QLatin1String("localhost") || url.host() == QLatin1String("127.0.0.1") ||
     url.host() == QLatin1String("::1"));
  if(!url.isValid() || (!m_manifestUrl.isEmpty() && !local) ||
     !url.userInfo().isEmpty() || url.hasQuery() || url.hasFragment() ||
     url.path() != QLatin1String("/updates/shadowchat.json"))
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
  QNetworkAccessManager* manager = new QNetworkAccessManager(this);
  manager->setProxy(QNetworkProxy::NoProxy);
  QNetworkRequest request(url);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
  request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
  request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
  request.setRawHeader("Accept", "application/json");
  request.setRawHeader("Cache-Control", "no-cache, no-store");
  QNetworkReply* reply = manager->get(request);
  reply->setReadBufferSize(65537);
  QTimer* timeout = new QTimer(reply);
  timeout->setSingleShot(true);
  connect(timeout, &QTimer::timeout, reply, [reply]() {
    reply->setProperty("shadowTimedOut", true);
    reply->abort();
  });
  timeout->start(8000);
  connect(reply, &QNetworkReply::readyRead, reply, [reply]() {
    if(reply->bytesAvailable() > 65536)
    {
      reply->setProperty("shadowOversized", true);
      reply->abort();
    }
  });
  connect(reply, &QNetworkReply::finished, this, [this, reply, manager]() {
    if(reply->property("shadowTimedOut").toBool())
      m_error = QStringLiteral("版本校验超时，请检查网络后重试。");
    else if(reply->property("shadowOversized").toBool())
      m_error = QStringLiteral("更新服务响应过大，无法校验版本。");
    else if(reply->error() != QNetworkReply::NoError)
      m_error = QStringLiteral("无法安全连接更新服务：%1").arg(reply->errorString());
    else if(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200)
      m_error = QStringLiteral("更新服务返回异常状态：%1。").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
    else
    {
      const QByteArray data = reply->readAll();
      m_manifestValid = ShadowUpdateManifest::parse(data, reply->url(), &m_versionAvailable, &m_downloadUrl, m_channel);
      if(!m_manifestValid)
        m_error = QStringLiteral("暂未获得本通道的有效发布信息。");
      else
      {
        const QJsonValue notes = QJsonDocument::fromJson(data).object().value("notes");
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
    }
    m_checking = false;
    reply->deleteLater();
    manager->deleteLater();
    emit jobCompleted();
  });
}

void Updater::onDownloadCompleted(const QString&) {}
