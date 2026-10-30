#ifndef SHADOWUPDATECONTROLLER_H
#define SHADOWUPDATECONTROLLER_H

#include <QtCore>
#include <functional>
class QWidget;
class QDialog;
class QLabel;
class QPushButton;
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
  QDialog* dialog() const { return m_dialog; }
  bool isChecking() const { return m_checking; }

public slots:
  void checkAutomatically();
  void checkManually();
  void presentPending();

signals:
  void checkFinished(bool valid);
  void promptShown(bool manual);

private:
  void check(bool manual);
  void completed();
  void showDialog(bool manual);
  void createDialog();
  bool claimToday();
  QString m_current;
  QWidget* m_window;
  Updater* m_updater;
  QString m_reminderFile;
  QString m_channel;
  QTimer m_timer;
  QDialog* m_dialog = nullptr;
  QLabel* m_status = nullptr;
  QPushButton* m_download = nullptr;
  QPushButton* m_retry = nullptr;
  QPushButton* m_dismiss = nullptr;
  std::function<bool()> m_busy;
  std::function<QDate()> m_date;
  bool m_checking = false;
  bool m_manual = false;
  bool m_pending = false;
  bool m_shuttingDown = false;
  bool m_discardResult = false;
};
#endif
