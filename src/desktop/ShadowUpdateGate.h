#ifndef SHADOWUPDATEGATE_H
#define SHADOWUPDATEGATE_H

#include <QDialog>
#include <QUrl>

class Updater;
class QLabel;
class QPushButton;
class QTimer;

class ShadowUpdateGate : public QDialog
{
  Q_OBJECT
public:
  explicit ShadowUpdateGate(const QString& currentVersion, QWidget* parent = nullptr,
                            const QUrl& testSource = QUrl());
  void startMonitoring();
  bool isBlocked() const { return m_runtimeBlocked; }

public slots:
  void checkNow();
  void accept() override;
  void reject() override;
  void done(int result) override;

signals:
  void runtimeBlocked();
  void exitRequested();

protected:
  void closeEvent(QCloseEvent* event) override;

private:
  void completed();
  void download();
  void requestExit();
  QString m_currentVersion;
  Updater* m_updater;
  QLabel* m_status;
  QPushButton* m_download;
  QPushButton* m_retry;
  QPushButton* m_exit;
  QTimer* m_timer;
  bool m_checking = false;
  bool m_verified = false;
  bool m_monitoring = false;
  bool m_runtimeBlocked = false;
};

#endif
