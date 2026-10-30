#include "ShadowUpdateController.h"
#include "ShadowDialogStyle.h"
#include "Updater.h"
#include "../core/ShadowBuild.h"
#include <QDesktopServices>

ShadowUpdateController::ShadowUpdateController(const QString& currentVersion, QWidget* window,
  const QUrl& testSource, const QString& reminderFile, const QString& testChannel)
  : QObject(window), m_current(currentVersion), m_window(window),
    m_updater(new Updater(this, testSource, testChannel)),
    m_reminderFile(reminderFile.isEmpty() ?
      QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("update-reminders.ini") : reminderFile),
    m_channel(testChannel.isEmpty() ? QString::fromLatin1(ShadowBuild::channel()) : testChannel),
    m_date([]() { return QDate::currentDate(); })
{
  connect(m_updater, &Updater::jobCompleted, this, &ShadowUpdateController::completed);
  connect(qApp, &QCoreApplication::aboutToQuit, this, &ShadowUpdateController::shutdown);
  m_timer.setInterval(5 * 60 * 1000);
  connect(&m_timer, &QTimer::timeout, this, &ShadowUpdateController::checkAutomatically);
}

ShadowUpdateController::~ShadowUpdateController()
{
  delete m_dialog;
}

void ShadowUpdateController::start()
{
  m_timer.start();
  QTimer::singleShot(5000, this, &ShadowUpdateController::checkAutomatically);
}

void ShadowUpdateController::shutdown()
{
  m_shuttingDown = true;
  m_pending = false;
  m_timer.stop();
  if(m_dialog)
    m_dialog->hide();
}

void ShadowUpdateController::checkAutomatically() { check(false); }
void ShadowUpdateController::checkManually() { check(true); }

void ShadowUpdateController::check(bool manual)
{
  if(m_shuttingDown)
    return;
  if(m_dialog && m_dialog->isVisible())
  {
    if(manual && !m_checking)
    {
      m_dialog->raise();
      m_dialog->activateWindow();
    }
    return;
  }
  if(m_checking)
  {
    if(manual)
    {
      m_manual = true;
      m_discardResult = false;
      showDialog(true);
    }
    return;
  }
  if(!manual && m_pending)
  {
    presentPending();
    return;
  }
  m_pending = false;
  m_manual = manual;
  m_discardResult = false;
  m_checking = true;
  if(manual)
    showDialog(true);
  m_updater->checkForNewVersion();
}

void ShadowUpdateController::completed()
{
  if(!m_checking)
    return;
  m_checking = false;
  if(m_shuttingDown || m_discardResult)
  {
    emit checkFinished(m_updater->manifestValid());
    return;
  }
  if(m_manual)
    showDialog(true);
  else if(m_updater->hasNewerVersion(m_current))
  {
    m_pending = true;
    presentPending();
  }
  emit checkFinished(m_updater->manifestValid());
}

bool ShadowUpdateController::claimToday()
{
  QDir().mkpath(QFileInfo(m_reminderFile).absolutePath());
  QLockFile lock(m_reminderFile + ".claim.lock");
  if(!lock.tryLock(0))
    return false;
  QSettings settings(m_reminderFile, QSettings::IniFormat);
  settings.sync();
  const QString key = m_channel + "/lastReminderDate";
  const QDate previous = QDate::fromString(settings.value(key).toString(), Qt::ISODate);
  const QDate today = m_date();
  if(!today.isValid() || (previous.isValid() && today <= previous))
    return false;
  settings.setValue(key, today.toString(Qt::ISODate));
  settings.sync();
  return settings.status() == QSettings::NoError;
}

void ShadowUpdateController::presentPending()
{
  if(m_shuttingDown || !m_pending || m_checking || (m_dialog && m_dialog->isVisible()))
    return;
  if((m_busy && m_busy()) || (m_window && !m_window->isVisible()) || QApplication::activeModalWidget())
    return;
  m_pending = false;
  if(claimToday())
    showDialog(false);
}

void ShadowUpdateController::createDialog()
{
  if(m_dialog)
    return;
  m_dialog = new QDialog(m_window);
  m_dialog->setObjectName("ShadowUpdateSuggestion");
  m_dialog->setWindowTitle(QStringLiteral("影聊 · 检查更新"));
  m_dialog->setWindowModality(Qt::NonModal);
  m_dialog->setMinimumWidth(540);
  ShadowDialogStyle::apply(m_dialog);
  QVBoxLayout* layout = new QVBoxLayout(m_dialog);
  layout->setContentsMargins(28, 24, 28, 24);
  layout->setSpacing(18);
  QLabel* title = new QLabel(QStringLiteral("影聊更新"), m_dialog);
  title->setProperty("role", "title");
  layout->addWidget(title);
  m_status = new QLabel(m_dialog);
  m_status->setObjectName("updateStatus");
  m_status->setWordWrap(true);
  m_status->setTextFormat(Qt::PlainText);
  m_status->setAccessibleName(QStringLiteral("更新检查结果"));
  layout->addWidget(m_status);
  QLabel* hint = new QLabel(QStringLiteral("当前版本 %1 · %2 / %3\n聊天和文件传输不受影响。\n下载后请自行覆盖安装，保留账号、历史、附件和待发队列。")
    .arg(m_current, QString::fromLatin1(ShadowBuild::environment()), m_channel), m_dialog);
  hint->setWordWrap(true);
  layout->addWidget(hint);
  QHBoxLayout* buttons = new QHBoxLayout;
  m_download = new QPushButton(QStringLiteral("立即下载(&D)"), m_dialog);
  m_retry = new QPushButton(QStringLiteral("重试(&R)"), m_dialog);
  m_dismiss = new QPushButton(QStringLiteral("今天不再提醒(&T)"), m_dialog);
  QPushButton* close = new QPushButton(QStringLiteral("关闭(&C)"), m_dialog);
  m_download->setObjectName("downloadUpdate");
  m_retry->setObjectName("retryUpdate");
  m_dismiss->setObjectName("dismissUpdate");
  close->setObjectName("closeUpdate");
  m_download->setProperty("role", "primary");
  for(QPushButton* button : {m_download, m_retry, m_dismiss, close})
  {
    button->setAutoDefault(false);
    buttons->addWidget(button);
  }
  layout->addLayout(buttons);
  connect(close, &QPushButton::clicked, m_dialog, &QDialog::reject);
  connect(m_dismiss, &QPushButton::clicked, this, [this]() {
    claimToday();
    m_dialog->reject();
  });
  connect(m_retry, &QPushButton::clicked, this, [this]() {
    m_dialog->hide();
    checkManually();
  });
  connect(m_download, &QPushButton::clicked, this, [this]() {
    if(!m_checking && m_updater->hasNewerVersion(m_current))
    {
      if(QDesktopServices::openUrl(QUrl(m_updater->downloadUrl())))
        m_dialog->close();
      else
        m_status->setText(QStringLiteral("无法打开下载页面，请稍后重试。聊天和文件传输不受影响。"));
    }
  });
  connect(m_dialog, &QDialog::finished, this, [this]() {
    if(m_checking)
      m_discardResult = true;
    m_manual = false;
    m_pending = false;
  });
}

void ShadowUpdateController::showDialog(bool manual)
{
  if(m_shuttingDown)
    return;
  createDialog();
  const bool wasVisible = m_dialog->isVisible();
  const bool newer = !m_checking && m_updater->hasNewerVersion(m_current);
  if(manual && newer)
    claimToday();
  m_download->setVisible(newer);
  m_dismiss->setVisible(newer);
  m_retry->setVisible(!m_checking && !m_updater->manifestValid());
  if(m_checking)
    m_status->setText(QStringLiteral("正在检查本通道的新版本… 可关闭此窗口继续使用。"));
  else if(!m_updater->manifestValid())
    m_status->setText(QStringLiteral("暂时无法检查更新：%1\n可以重试，聊天和文件传输不受影响。").arg(m_updater->errorString()));
  else if(newer)
    m_status->setText(QStringLiteral("影聊 %1 已发布，建议在方便时更新。%2").arg(m_updater->versionAvailable(),
      m_updater->news().isEmpty() ? QString() : "\n" + m_updater->news()));
  else
    m_status->setText(QStringLiteral("当前已是最新版本。"));
  m_dialog->setAttribute(Qt::WA_ShowWithoutActivating, !manual);
  m_dialog->show();
  if(manual && !wasVisible)
  {
    m_dialog->raise();
    m_dialog->activateWindow();
    (m_retry->isVisible() ? m_retry : m_download->isVisible() ? m_download :
      m_dialog->findChild<QPushButton*>("closeUpdate"))->setFocus();
  }
  if(!wasVisible)
    emit promptShown(manual);
}
