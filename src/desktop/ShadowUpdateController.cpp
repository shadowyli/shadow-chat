#include "ShadowUpdateController.h"
#include "ShadowDialogStyle.h"
#include "Updater.h"
#include "../core/ShadowBuild.h"
#include <QApplication>
#include <QDesktopServices>
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QRandomGenerator>
#include <QSettings>
#include <QLockFile>
#include <QVBoxLayout>

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
  m_timer.setInterval(6 * 60 * 60 * 1000 + QRandomGenerator::global()->bounded(20 * 60 * 1000));
  connect(&m_timer, &QTimer::timeout, this, &ShadowUpdateController::checkAutomatically);
}

ShadowUpdateController::~ShadowUpdateController()
{
  delete m_dialog;
}

void ShadowUpdateController::start()
{
  m_timer.start();
  QTimer::singleShot(45 * 1000 + QRandomGenerator::global()->bounded(45 * 1000),
    this, &ShadowUpdateController::checkAutomatically);
}

void ShadowUpdateController::shutdown()
{
  m_shuttingDown = true;
  m_pending = false;
  m_timer.stop();
  if(m_dialog)
    m_dialog->hide();
}

void ShadowUpdateController::checkAutomatically()
{
  if(m_shuttingDown || m_checking)
    return;
  if(alreadyRemindedToday())
  {
    emit statusChanged();
    emit checkFinished(m_updater->manifestValid());
    return;
  }
  check(false, true);
}

void ShadowUpdateController::checkManually() { check(true, true); }
void ShadowUpdateController::checkManuallyInline() { check(true, false); }

void ShadowUpdateController::cancelInlinePresentation()
{
  if(m_manual && !m_showPrompt && m_checking)
  {
    m_manual = false;
    m_discardResult = true;
  }
}

void ShadowUpdateController::check(bool manual, bool show_prompt)
{
  if(m_shuttingDown)
    return;
  if(manual && !show_prompt && m_dialog && m_dialog->isVisible())
    m_dialog->hide();
  if(m_dialog && m_dialog->isVisible())
  {
    if(manual && show_prompt && !m_checking)
    {
      m_dialog->raise();
      m_dialog->activateWindow();
    }
    emit statusChanged();
    return;
  }
  if(m_checking)
  {
    if(manual)
    {
      m_manual = true;
      m_showPrompt = show_prompt;
      m_discardResult = false;
      if(show_prompt)
        showDialog(true);
    }
    emit statusChanged();
    return;
  }
  if(!manual && m_pending)
  {
    presentPending();
    emit statusChanged();
    return;
  }
  m_pending = false;
  m_manual = manual;
  m_showPrompt = show_prompt;
  m_discardResult = false;
  m_checking = true;
  if(manual && show_prompt)
    showDialog(true);
  emit statusChanged();
  m_updater->checkForNewVersion();
}

void ShadowUpdateController::completed()
{
  if(!m_checking)
    return;
  m_checking = false;
  if(m_shuttingDown || m_discardResult)
  {
    emit statusChanged();
    emit checkFinished(m_updater->manifestValid());
    return;
  }
  const bool newer = m_updater->hasNewerVersion(m_current);
  if(m_manual)
  {
    if(newer)
      claimToday();
    if(m_showPrompt)
      showDialog(true);
    else
      emit statusChanged();
  }
  else if(newer)
  {
    m_pending = true;
    presentPending();
    emit statusChanged();
  }
  else
    emit statusChanged();
  emit checkFinished(m_updater->manifestValid());
}

QString ShadowUpdateController::reminderKey() const
{
  return m_channel + QStringLiteral("/lastReminderDate");
}

bool ShadowUpdateController::alreadyRemindedToday() const
{
  QSettings settings(m_reminderFile, QSettings::IniFormat);
  const QDate previous = QDate::fromString(settings.value(reminderKey()).toString(), Qt::ISODate);
  const QDate today = m_date();
  return today.isValid() && previous.isValid() && today <= previous;
}

bool ShadowUpdateController::claimToday()
{
  QDir().mkpath(QFileInfo(m_reminderFile).absolutePath());
  QLockFile lock(m_reminderFile + ".claim.lock");
  if(!lock.tryLock(0))
    return false;
  QSettings settings(m_reminderFile, QSettings::IniFormat);
  settings.sync();
  const QDate previous = QDate::fromString(settings.value(reminderKey()).toString(), Qt::ISODate);
  const QDate today = m_date();
  if(!today.isValid() || (previous.isValid() && today <= previous))
    return false;
  settings.setValue(reminderKey(), today.toString(Qt::ISODate));
  settings.sync();
  return settings.status() == QSettings::NoError;
}

QString ShadowUpdateController::statusText() const
{
  if(m_checking)
    return QStringLiteral("正在检查本通道的新版本… 可继续使用影聊。");
  if(!m_updater->manifestValid())
  {
    if(m_updater->errorString().isEmpty())
      return QStringLiteral("尚未检查更新。");
    return QStringLiteral("暂时无法检查更新：%1\n可以重试，聊天和文件传输不受影响。")
      .arg(m_updater->errorString());
  }
  if(m_updater->hasNewerVersion(m_current))
    return QStringLiteral("影聊 %1 已发布，建议在方便时更新。").arg(m_updater->versionAvailable());
  return QStringLiteral("当前已是最新版本。");
}

bool ShadowUpdateController::statusCanDownload() const
{
  return !m_checking && m_updater->hasNewerVersion(m_current);
}

bool ShadowUpdateController::statusCanRetry() const
{
  return !m_checking;
}

QString ShadowUpdateController::downloadUrl() const
{
  return m_updater->downloadUrl();
}

QString ShadowUpdateController::news() const
{
  return m_updater->news();
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
  m_notes = new QPlainTextEdit(m_dialog);
  m_notes->setObjectName("updateNotes");
  m_notes->setReadOnly(true);
  m_notes->setAccessibleName(QStringLiteral("版本更新说明"));
  m_notes->setFixedHeight(100);
  layout->addWidget(m_notes);
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
  for(QPushButton* button : {m_download.data(), m_retry.data(), m_dismiss.data(), close})
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
        m_status->setText(QStringLiteral("无法开始下载安装包，请稍后重试。聊天和文件传输不受影响。"));
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
  if(!m_dialog || !m_status || !m_download || !m_retry || !m_dismiss || !m_notes)
    return;
  const bool wasVisible = m_dialog->isVisible();
  const bool newer = !m_checking && m_updater->hasNewerVersion(m_current);
  if(manual && newer)
    claimToday();
  m_download->setVisible(newer);
  m_dismiss->setVisible(newer);
  m_retry->setVisible(!m_checking && !m_updater->manifestValid());
  m_notes->setPlainText(m_updater->news());
  m_notes->setVisible(newer && !m_updater->news().isEmpty());
  if(m_checking)
    m_status->setText(QStringLiteral("正在检查本通道的新版本… 可关闭此窗口继续使用。"));
  else if(!m_updater->manifestValid())
    m_status->setText(QStringLiteral("暂时无法检查更新：%1\n可以重试，聊天和文件传输不受影响。").arg(m_updater->errorString()));
  else if(newer)
    m_status->setText(statusText());
  else
    m_status->setText(QStringLiteral("当前已是最新版本。"));
  m_dialog->setAttribute(Qt::WA_ShowWithoutActivating, !manual);
  m_dialog->show();
  if(manual && !wasVisible)
  {
    m_dialog->raise();
    m_dialog->activateWindow();
    QPushButton* focus = m_retry->isVisible() ? m_retry.data() :
      (m_download->isVisible() ? m_download.data() :
       m_dialog->findChild<QPushButton*>("closeUpdate"));
    if(focus)
      focus->setFocus();
  }
  emit statusChanged();
  if(!wasVisible)
    emit promptShown(manual);
}
