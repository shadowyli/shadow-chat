#ifndef SHADOWUPDATECONTROLLER_H
#define SHADOWUPDATECONTROLLER_H

#include <QtCore>
#include <QPointer>
#include <QDialog>
#include <functional>
class QWidget;
class QDialog;
class QLabel;
class QPushButton;
class QPlainTextEdit;
class Updater;

class ShadowUpdateController : public QObject
{
  Q_OBJECT
public:
  explicit ShadowUpdateController(const QString& currentVersion, QWidget* window,
    const QUrl& testSource = QUrl(), const QString& reminderFile = QString(),
    const QString& testChannel = QString());
  ~ShadowUpdateController() override;
  void setBusyPredicate(const std::function<bool()>& predicate) { m_busy = predicate; }
  void setDateProvider(const std::function<QDate()>& provider) { m_date = provider; }
  void start();
  void shutdown();
  QDialog* dialog() const { return m_dialog.data(); }
  bool isChecking() const { return m_checking; }
  QString statusText() const;
  bool statusCanDownload() const;
  bool statusCanRetry() const;
  QString downloadUrl() const;
  QString news() const;

public slots:
  void checkAutomatically();
  void checkManually();
  void checkManuallyInline();
  void cancelInlinePresentation();
  void presentPending();

signals:
  void checkFinished(bool valid);
  void promptShown(bool manual);
  void statusChanged();

private:
  void check(bool manual, bool show_prompt);
  void completed();
  void showDialog(bool manual);
  void createDialog();
  bool claimToday();
  bool alreadyRemindedToday() const;
  QString reminderKey() const;
  QString m_current;
  QWidget* m_window;
  Updater* m_updater;
  QString m_reminderFile;
  QString m_channel;
  QTimer m_timer;
  QPointer<QDialog> m_dialog;
  QPointer<QLabel> m_status;
  QPointer<QPushButton> m_download;
  QPointer<QPushButton> m_retry;
  QPointer<QPushButton> m_dismiss;
  QPointer<QPlainTextEdit> m_notes;
  std::function<bool()> m_busy;
  std::function<QDate()> m_date;
  bool m_checking = false;
  bool m_manual = false;
  bool m_showPrompt = true;
  bool m_pending = false;
  bool m_shuttingDown = false;
  bool m_discardResult = false;
};
#endif
