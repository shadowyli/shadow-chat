#include "ShadowUpdateGate.h"
#include "ShadowDialogStyle.h"
#include "Updater.h"
#include <QCloseEvent>
#include <QDesktopServices>
#include <QTimer>

ShadowUpdateGate::ShadowUpdateGate(const QString& currentVersion, QWidget* parent, const QUrl& testSource)
  : QDialog(parent), m_currentVersion(currentVersion), m_updater(new Updater(this, testSource)),
    m_status(new QLabel(this)), m_download(new QPushButton(QStringLiteral("下载更新(&D)"), this)),
    m_retry(new QPushButton(QStringLiteral("重试校验(&R)"), this)),
    m_exit(new QPushButton(QStringLiteral("退出(&X)"), this)), m_timer(new QTimer(this))
{
  setObjectName(QStringLiteral("ShadowUpdateGate"));
  setWindowTitle(QStringLiteral("影聊 · 版本校验"));
  setWindowModality(Qt::ApplicationModal);
  setMinimumWidth(520);
  ShadowDialogStyle::apply(this);
  QVBoxLayout* layout = new QVBoxLayout(this);
  layout->setContentsMargins(28, 24, 28, 24);
  layout->setSpacing(18);
  QLabel* title = new QLabel(QStringLiteral("使用前必须确认版本"), this);
  title->setProperty("role", "title");
  layout->addWidget(title);
  m_status->setObjectName(QStringLiteral("updateStatus"));
  m_status->setWordWrap(true);
  m_status->setTextFormat(Qt::PlainText);
  m_status->setAccessibleName(QStringLiteral("版本校验状态"));
  layout->addWidget(m_status);
  QLabel* hint = new QLabel(QStringLiteral("当前版本：%1\n只有成功确认当前版本不低于最新发布版，才可进入影聊。\n更新请覆盖安装并保留用户资料；请勿卸载或删除资料目录。").arg(currentVersion), this);
  hint->setWordWrap(true);
  layout->addWidget(hint);
  QHBoxLayout* buttons = new QHBoxLayout;
  m_download->setObjectName(QStringLiteral("downloadUpdate"));
  m_retry->setObjectName(QStringLiteral("retryUpdate"));
  m_exit->setObjectName(QStringLiteral("exitUpdate"));
  m_download->setProperty("role", "primary");
  for(QPushButton* button : {m_download, m_retry, m_exit})
  {
    button->setAutoDefault(false);
    buttons->addWidget(button);
  }
  layout->addLayout(buttons);
  setTabOrder(m_download, m_retry);
  setTabOrder(m_retry, m_exit);
  m_download->setEnabled(false);
  connect(m_download, &QPushButton::clicked, this, &ShadowUpdateGate::download);
  connect(m_retry, &QPushButton::clicked, this, &ShadowUpdateGate::checkNow);
  connect(m_exit, &QPushButton::clicked, this, &ShadowUpdateGate::requestExit);
  connect(m_updater, &Updater::jobCompleted, this, &ShadowUpdateGate::completed);
  m_timer->setInterval(5 * 60 * 1000);
  connect(m_timer, &QTimer::timeout, this, &ShadowUpdateGate::checkNow);
}

void ShadowUpdateGate::startMonitoring()
{
  m_monitoring = true;
  m_timer->start();
}

void ShadowUpdateGate::checkNow()
{
  if(m_checking)
    return;
  m_checking = true;
  m_verified = false;
  m_retry->setEnabled(false);
  m_download->setEnabled(false);
  m_status->setText(QStringLiteral("检查中… 正在安全连接发布服务，请稍候。"));
  if(!m_monitoring || m_runtimeBlocked)
  {
    show();
    raise();
    m_exit->setFocus();
  }
  m_updater->checkForNewVersion();
}

void ShadowUpdateGate::completed()
{
  m_checking = false;
  m_retry->setEnabled(true);
  m_verified = m_updater->allowsVersion(m_currentVersion);
  if(m_verified && !m_runtimeBlocked)
  {
    if(!m_monitoring)
      accept();
    return;
  }
  if(m_monitoring && !m_runtimeBlocked)
  {
    m_runtimeBlocked = true;
    emit runtimeBlocked();
  }
  m_download->setEnabled(m_updater->manifestValid());
  if(m_verified)
    m_status->setText(QStringLiteral("版本已确认。聊天网络已停止，为保护会话状态，请退出并重新启动影聊后使用。"));
  else if(m_updater->manifestValid())
    m_status->setText(QStringLiteral("必须更新：影聊 %1 已发布，当前版本不能继续使用。\n%2").arg(m_updater->versionAvailable(),
      m_runtimeBlocked ? QStringLiteral("聊天网络已停止。请下载更新，退出影聊后覆盖安装。") : QStringLiteral("请下载更新，退出影聊后覆盖安装。")));
  else
    m_status->setText(QStringLiteral("无法确认版本，已阻止使用。\n%1\n请检查网络或联系管理员，然后重试。").arg(m_updater->errorString()));
  if(m_runtimeBlocked)
    m_status->setText(m_status->text() + QStringLiteral("\n未发送消息和传输队列资料会保留；正在传输的文件可能需要重试。"));
  show();
  raise();
  activateWindow();
  (m_download->isEnabled() ? m_download : m_retry)->setFocus();
}

void ShadowUpdateGate::download()
{
  if(!m_checking && m_updater->manifestValid())
  {
    if(!QDesktopServices::openUrl(QUrl(m_updater->downloadUrl())))
      m_status->setText(QStringLiteral("无法打开下载页面，请重试或联系管理员。当前仍禁止使用影聊。"));
  }
}

void ShadowUpdateGate::accept()
{
  done(QDialog::Accepted);
}

void ShadowUpdateGate::done(int result)
{
  if(result == QDialog::Accepted && m_verified && !m_checking && !m_runtimeBlocked)
    QDialog::done(result);
}

void ShadowUpdateGate::requestExit()
{
  m_timer->stop();
  emit exitRequested();
  if(!m_monitoring)
    QDialog::done(QDialog::Rejected);
}

void ShadowUpdateGate::reject()
{
  requestExit();
}

void ShadowUpdateGate::closeEvent(QCloseEvent* event)
{
  event->ignore();
  requestExit();
}
