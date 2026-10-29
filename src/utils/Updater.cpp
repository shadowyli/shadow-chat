#include "Updater.h"
#include "ShadowUpdateManifest.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QSslSocket>
#include <QTimer>

Updater::Updater(QObject* parent, const QUrl& manifest_url)
  : QObject(parent), m_manifestUrl(manifest_url)
{
  setObjectName("Updater");
}

QUrl Updater::productionManifestUrl()
{
  return QUrl(QStringLiteral("https://flowx.gz/updates/shadowchat.json"));
}

bool Updater::allowsVersion(const QString& current) const
{
  return m_manifestValid && ShadowUpdateManifest::allowsVersion(current, m_versionAvailable);
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
  const bool local = url.scheme() == QLatin1String("http") &&
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
      m_manifestValid = ShadowUpdateManifest::parse(reply->readAll(), reply->url(), &m_versionAvailable, &m_downloadUrl);
      if(!m_manifestValid)
        m_error = QStringLiteral("发布清单未发布或内容无效，暂时无法确认当前版本。");
    }
    m_checking = false;
    reply->deleteLater();
    manager->deleteLater();
    emit jobCompleted();
  });
}

void Updater::onDownloadCompleted(const QString&) {}
