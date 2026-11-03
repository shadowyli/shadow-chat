#include "ShadowUpdateController.h"
#include "Updater.h"
#include "BeeApplication.h"
#include "Core.h"
#include "GuiMain.h"
#include "Settings.h"
#include "ShadowStyle.h"
#include "ShadowDialogChrome.h"
#include "Random.h"
#include <QtTest>
#include <QtWidgets>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QDesktopServices>

class UpdateSuggestionTest : public QObject
{
  Q_OBJECT
  QTcpServer m_server;
  QByteArray m_body;
  int m_requests = 0;
  int m_opened = 0;
  int m_delay = 0;
  QUrl m_openedUrl;
  QTemporaryDir m_profile;

  QByteArray manifest(const QString& version = "1.1.3", const QString& channel = "test")
  {
    QJsonObject object;
    object["schemaVersion"] = 1;
    object["product"] = "shadowchat";
    object["published"] = true;
    object["version"] = version;
    object["channel"] = channel;
    object["mandatory"] = true;
    object["notes"] = QStringLiteral("改进文件传输体验，保留现有资料。");
    object["downloadPage"] = "/tools/shadowchat";
    object["installerUrl"] = "/downloads/fixture.exe";
    object["sha256"] = QString(64, 'a');
    return QJsonDocument(object).toJson();
  }
  QUrl source() const
  {
    return QUrl(QStringLiteral("http://127.0.0.1:%1/updates/shadowchat.json").arg(m_server.serverPort()));
  }
  void check(ShadowUpdateController& controller, bool manual = false)
  {
    QSignalSpy finished(&controller, &ShadowUpdateController::checkFinished);
    if(manual) controller.checkManually(); else controller.checkAutomatically();
    QTRY_COMPARE(finished.count(), 1);
  }
  void screenshot(ShadowUpdateController& controller, const QString& name)
  {
    const QString folder = qEnvironmentVariable("SHADOW_UPDATE_SCREENSHOT_DIR");
    if(folder.isEmpty()) return;
    QVERIFY(QDir().mkpath(folder));
    QVERIFY(controller.dialog());
    QVERIFY(controller.dialog()->grab().save(QDir(folder).filePath(name + ".png")));
  }

private slots:
  void initTestCase()
  {
    QVERIFY(m_profile.isValid());
    qApp->setProperty("shadowAccountRoot", m_profile.path());
    Random::init();
    Settings::instance().setDefaultFolders();
    Settings::instance().createLocalUser("synthetic-update-test");
    Settings::instance().setAccountDataFolder(m_profile.path());
    Settings::instance().load();
    QVERIFY(m_server.listen(QHostAddress::LocalHost));
    connect(&m_server, &QTcpServer::newConnection, this, [this]() {
      while(m_server.hasPendingConnections())
      {
        QTcpSocket* socket = m_server.nextPendingConnection();
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
          QByteArray request = socket->property("request").toByteArray() + socket->readAll();
          socket->setProperty("request", request);
          if(!request.contains("\r\n\r\n") || socket->property("sent").toBool()) return;
          socket->setProperty("sent", true);
          ++m_requests;
          const QByteArray response = m_body;
          QTimer::singleShot(m_delay, socket, [socket, response]() {
            socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
                          QByteArray::number(response.size()) + "\r\nConnection: close\r\n\r\n" + response);
            socket->disconnectFromHost();
          });
        });
      }
    });
    QDesktopServices::setUrlHandler("http", this, "openFixture");
    QDesktopServices::setUrlHandler("https", this, "openFixture");
  }
  void openFixture(const QUrl& url) { ++m_opened; m_openedUrl = url; }
  void init() { m_body = manifest(); m_delay = 0; }

  void versions_data()
  {
    QTest::addColumn<QString>("current");
    QTest::addColumn<QString>("released");
    QTest::addColumn<QString>("channel");
    QTest::addColumn<bool>("suggest");
    QTest::newRow("newer-mandatory-true") << "1.1.2" << "1.1.3" << "test" << true;
    QTest::newRow("current") << "1.1.2" << "1.1.2" << "test" << false;
    QTest::newRow("higher-development") << "2.0.0" << "1.1.3" << "test" << false;
    QTest::newRow("wrong-channel") << "1.1.2" << "1.1.3" << "stable" << false;
    QTest::newRow("overflow") << "1.1.2" << "2147483648.0.0" << "test" << false;
  }
  void versions()
  {
    QFETCH(QString, current); QFETCH(QString, released); QFETCH(QString, channel); QFETCH(bool, suggest);
    QTemporaryDir temporary;
    m_body = manifest(released, channel);
    ShadowUpdateController controller(current, nullptr, source(), temporary.filePath("reminders.ini"));
    QSignalSpy shown(&controller, &ShadowUpdateController::promptShown);
    check(controller);
    QCOMPARE(shown.count(), suggest ? 1 : 0);
    if(suggest) QCOMPARE(controller.dialog()->windowModality(), Qt::NonModal);
  }
  void dailyPersistenceAndRollback()
  {
    QTemporaryDir temporary;
    const QString file = temporary.filePath("reminders.ini");
    QDate day(2026, 10, 29);
    {
      ShadowUpdateController first("1.1.2", nullptr, source(), file);
      first.setDateProvider([&]() { return day; });
      check(first);
      QVERIFY(first.dialog()->isVisible());
      QTest::keyClick(first.dialog(), Qt::Key_Escape);
      QVERIFY(!first.dialog()->isVisible());
      m_body = manifest("1.1.4");
      QSignalSpy shown(&first, &ShadowUpdateController::promptShown);
      check(first);
      QCOMPARE(shown.count(), 0);
    }
    ShadowUpdateController restarted("1.1.2", nullptr, source(), file);
    restarted.setDateProvider([&]() { return day; });
    QSignalSpy shown(&restarted, &ShadowUpdateController::promptShown);
    check(restarted);
    QCOMPARE(shown.count(), 0);
    day = day.addDays(-1);
    check(restarted);
    QCOMPARE(shown.count(), 0);
    day = QDate(2026, 10, 30);
    check(restarted);
    QCOMPARE(shown.count(), 1);
    restarted.dialog()->findChild<QPushButton*>("dismissUpdate")->click();
    check(restarted, true);
    QCOMPARE(shown.count(), 2);
    restarted.dialog()->close();
  }
  void currentAcrossDaysAndRestart()
  {
    QTemporaryDir temporary;
    m_body = manifest("1.1.2");
    for(int offset = 0; offset < 3; ++offset)
    {
      ShadowUpdateController controller("1.1.2", nullptr, source(), temporary.filePath("reminders.ini"));
      controller.setDateProvider([offset]() { return QDate(2026, 10, 29).addDays(offset); });
      QSignalSpy shown(&controller, &ShadowUpdateController::promptShown);
      check(controller);
      QCOMPARE(shown.count(), 0);
    }
    m_body = manifest("1.1.3");
    ShadowUpdateController upgraded("1.1.3", nullptr, source(), temporary.filePath("reminders.ini"));
    QSignalSpy shown(&upgraded, &ShadowUpdateController::promptShown);
    check(upgraded);
    QCOMPARE(shown.count(), 0);
    QVERIFY(!QFile::exists(temporary.filePath("reminders.ini")));
  }
  void concurrentControllersAndChannels()
  {
    QTemporaryDir temporary;
    const QString file = temporary.filePath("reminders.ini");
    ShadowUpdateController first("1.1.2", nullptr, source(), file);
    ShadowUpdateController second("1.1.2", nullptr, source(), file);
    QSignalSpy firstShown(&first, &ShadowUpdateController::promptShown);
    QSignalSpy secondShown(&second, &ShadowUpdateController::promptShown);
    QSignalSpy firstFinished(&first, &ShadowUpdateController::checkFinished);
    QSignalSpy secondFinished(&second, &ShadowUpdateController::checkFinished);
    first.checkAutomatically();
    second.checkAutomatically();
    QTRY_COMPARE(firstFinished.count(), 1);
    QTRY_COMPARE(secondFinished.count(), 1);
    QCOMPARE(firstShown.count() + secondShown.count(), 1);
    if(first.dialog()) first.dialog()->close();
    if(second.dialog()) second.dialog()->close();
    m_body = manifest("1.1.3", "stable");
    ShadowUpdateController stable("1.1.2", nullptr, source(), file, "stable");
    QSignalSpy stableShown(&stable, &ShadowUpdateController::promptShown);
    check(stable);
    QCOMPARE(stableShown.count(), 1);
    QSettings settings(file, QSettings::IniFormat);
    QVERIFY(settings.contains("test/lastReminderDate"));
    QVERIFY(settings.contains("stable/lastReminderDate"));
  }
  void failedClaimCanRetry()
  {
    QTemporaryDir temporary;
    const QString file = temporary.filePath("reminders.ini");
    QLockFile lock(file + ".claim.lock");
    QVERIFY(lock.tryLock());
    ShadowUpdateController controller("1.1.2", nullptr, source(), file);
    QSignalSpy shown(&controller, &ShadowUpdateController::promptShown);
    check(controller);
    QCOMPARE(shown.count(), 0);
    QVERIFY(!QFile::exists(file));
    lock.unlock();
    check(controller);
    QCOMPARE(shown.count(), 1);
  }
  void errorsDoNotConsumeOpportunity()
  {
    QTemporaryDir temporary;
    ShadowUpdateController controller("1.1.2", nullptr, source(), temporary.filePath("reminders.ini"));
    m_body = "<html>service unavailable</html>";
    QSignalSpy shown(&controller, &ShadowUpdateController::promptShown);
    check(controller);
    QCOMPARE(shown.count(), 0);
    QVERIFY(!QFile::exists(temporary.filePath("reminders.ini")));
    check(controller, true);
    QVERIFY(controller.dialog()->findChild<QLabel*>("updateStatus")->text().contains(QStringLiteral("不受影响")));
    screenshot(controller, "manual-update-error");
    QVERIFY(controller.dialog()->findChild<QPushButton*>("retryUpdate")->isVisible());
    QVERIFY(!QFile::exists(temporary.filePath("reminders.ini")));
    m_body = manifest();
    QSignalSpy finished(&controller, &ShadowUpdateController::checkFinished);
    controller.dialog()->findChild<QPushButton*>("retryUpdate")->click();
    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(controller.dialog()->findChild<QPushButton*>("downloadUpdate")->isVisible());
    controller.dialog()->close();
  }
  void downloadAndCloseContinue()
  {
    QTemporaryDir temporary;
    QWidget work;
    work.show();
    ShadowUpdateController controller("1.1.2", &work, source(), temporary.filePath("reminders.ini"));
    check(controller);
    screenshot(controller, "daily-update-suggestion");
    QVERIFY(controller.dialog()->testAttribute(Qt::WA_ShowWithoutActivating));
    const int opened = m_opened;
    controller.dialog()->findChild<QPushButton*>("downloadUpdate")->click();
    QCOMPARE(m_opened, opened + 1);
    QCOMPARE(m_openedUrl, source().resolved(QUrl("/downloads/fixture.exe")));
    QVERIFY(work.isEnabled());
    QVERIFY(!controller.dialog()->isVisible());
    check(controller, true);
    controller.dialog()->findChild<QPushButton*>("closeUpdate")->click();
    QVERIFY(work.isEnabled());
  }
  void githubDownloadOpensInstaller()
  {
    QTemporaryDir temporary;
    const QString installer = "https://github.com/shadowyli/shadow-chat/releases/download/v1.1.3-test/ShadowChat-1.1.3-local-setup.exe";
    QJsonObject asset{
      {"name", "ShadowChat-1.1.3-local-setup.exe"}, {"size", 1000},
      {"digest", "sha256:" + QString(64, 'a')}, {"browser_download_url", installer},
    };
    QJsonObject release{
      {"tag_name", "v1.1.3-test"}, {"draft", false}, {"prerelease", true},
      {"body", QString(70000, 'x') + "\n- Direct installer download"}, {"assets", QJsonArray{asset}},
    };
    m_body = QJsonDocument(QJsonArray{release}).toJson();
    QUrl endpoint = source();
    endpoint.setPath("/repos/shadowyli/shadow-chat/releases");
    ShadowUpdateController controller("1.1.2", nullptr, endpoint, temporary.filePath("reminders.ini"));
    check(controller, true);
    QPushButton* download = controller.dialog()->findChild<QPushButton*>("downloadUpdate");
    QVERIFY(download->isVisible());
    const int opened = m_opened;
    download->click();
    QCOMPARE(m_opened, opened + 1);
    QCOMPARE(m_openedUrl, QUrl(installer));
    QVERIFY(!controller.dialog()->isVisible());
  }
  void oneRequestAndDialog()
  {
    QTemporaryDir temporary;
    m_delay = 80;
    ShadowUpdateController controller("1.1.2", nullptr, source(), temporary.filePath("reminders.ini"));
    QSignalSpy finished(&controller, &ShadowUpdateController::checkFinished);
    QSignalSpy shown(&controller, &ShadowUpdateController::promptShown);
    const int requests = m_requests;
    controller.checkAutomatically();
    for(int index = 0; index < 10; ++index) controller.checkAutomatically();
    controller.checkManually();
    controller.checkManually();
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(m_requests, requests + 1);
    QCOMPARE(shown.count(), 1);
    QMetaObject::invokeMethod(controller.findChild<Updater*>(), "jobCompleted", Qt::DirectConnection);
    QCOMPARE(shown.count(), 1);
    for(int index = 0; index < 5; ++index) controller.checkAutomatically();
    QCOMPARE(m_requests, requests + 1);
  }
  void closingInFlightAndExit()
  {
    QTemporaryDir temporary;
    m_delay = 80;
    ShadowUpdateController controller("1.1.2", nullptr, source(), temporary.filePath("reminders.ini"));
    QSignalSpy finished(&controller, &ShadowUpdateController::checkFinished);
    controller.checkManually();
    controller.dialog()->close();
    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(!controller.dialog()->isVisible());
    controller.checkAutomatically();
    controller.shutdown();
    QTRY_COMPARE(finished.count(), 2);
    QVERIFY(!controller.dialog()->isVisible());
    const int requests = m_requests;
    controller.checkManually();
    controller.checkAutomatically();
    QTest::qWait(50);
    QCOMPARE(m_requests, requests);
  }
  void transfersDeferAutomaticOnly()
  {
    QTemporaryDir temporary;
    bool transferring = true;
    ShadowUpdateController controller("1.1.2", nullptr, source(), temporary.filePath("reminders.ini"));
    controller.setBusyPredicate([&]() { return transferring; });
    QSignalSpy shown(&controller, &ShadowUpdateController::promptShown);
    check(controller);
    QCOMPARE(shown.count(), 0);
    QVERIFY(!QFile::exists(temporary.filePath("reminders.ini")));
    controller.presentPending();
    QCOMPARE(shown.count(), 0);
    transferring = false;
    controller.presentPending();
    QCOMPARE(shown.count(), 1);
  }
  void manualCurrent()
  {
    QTemporaryDir temporary;
    m_body = manifest("1.1.2");
    ShadowUpdateController controller("1.1.2", nullptr, source(), temporary.filePath("reminders.ini"));
    check(controller, true);
    QVERIFY(controller.dialog()->findChild<QLabel*>("updateStatus")->text().contains(QStringLiteral("当前已是最新版本")));
    QVERIFY(!controller.dialog()->findChild<QPushButton*>("downloadUpdate")->isVisible());
  }
  void manualDismissPersists()
  {
    QTemporaryDir temporary;
    const QString file=temporary.filePath("reminders.ini");
    {
      ShadowUpdateController manual("1.1.2",nullptr,source(),file);
      check(manual,true);
      manual.dialog()->findChild<QPushButton*>("dismissUpdate")->click();
      QSignalSpy shown(&manual,&ShadowUpdateController::promptShown);
      check(manual);
      QCOMPARE(shown.count(),0);
    }
    ShadowUpdateController restarted("1.1.2",nullptr,source(),file);
    QSignalSpy shown(&restarted,&ShadowUpdateController::promptShown);
    check(restarted);
    QCOMPARE(shown.count(),0);
    check(restarted,true);
    QCOMPARE(shown.count(),1);
  }
  void closeAndImmediatelyReopenInFlight()
  {
    QTemporaryDir temporary;
    m_delay=150;
    ShadowUpdateController controller("1.1.2",nullptr,source(),temporary.filePath("reminders.ini"));
    QSignalSpy finished(&controller,&ShadowUpdateController::checkFinished);
    const int requests=m_requests;
    controller.checkManually();
    controller.dialog()->close();
    QVERIFY(controller.isChecking());
    controller.checkManually();
    QTRY_COMPARE(finished.count(),1);
    QCOMPARE(m_requests,requests+1);
    QVERIFY(controller.dialog()->isVisible());
    QVERIFY(controller.dialog()->findChild<QPushButton*>("downloadUpdate")->isVisible());
    QVERIFY(!controller.dialog()->findChild<QLabel*>("updateStatus")->text().contains(QStringLiteral("正在检查")));
  }
  void inlineReplacesVisiblePrompt()
  {
    QTemporaryDir temporary;
    ShadowUpdateController controller("1.1.2", nullptr, source(), temporary.filePath("reminders.ini"));
    check(controller, true);
    QVERIFY(controller.dialog()->isVisible());
    m_body = "invalid";
    QSignalSpy finished(&controller, &ShadowUpdateController::checkFinished);
    controller.checkManuallyInline();
    QVERIFY(controller.isChecking());
    QVERIFY(!controller.dialog()->isVisible());
    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(!controller.statusCanDownload());
    QVERIFY(controller.statusCanRetry());
    QVERIFY(controller.statusText().contains(QStringLiteral("暂时无法检查")));
    QVERIFY(!controller.statusText().contains(QStringLiteral("已是最新")));
  }
  void abandonedInlineDoesNotClaim()
  {
    QTemporaryDir temporary;
    m_delay = 100;
    const QString file = temporary.filePath("reminders.ini");
    ShadowUpdateController controller("1.1.2", nullptr, source(), file);
    QSignalSpy finished(&controller, &ShadowUpdateController::checkFinished);
    controller.checkManuallyInline();
    controller.cancelInlinePresentation();
    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(!QFile::exists(file));
    QVERIFY(!controller.dialog());
    QSignalSpy shown(&controller, &ShadowUpdateController::promptShown);
    check(controller);
    QCOMPARE(shown.count(), 1);
  }
  void modalDefersWithoutClaim()
  {
    QTemporaryDir temporary;
    const QString file = temporary.filePath("reminders.ini");
    QWidget window;
    window.show();
    ShadowUpdateController controller("1.1.2", &window, source(), file);
    QDialog modal(&window);
    modal.setWindowModality(Qt::ApplicationModal);
    modal.show();
    QTRY_COMPARE(QApplication::activeModalWidget(), &modal);
    QSignalSpy shown(&controller, &ShadowUpdateController::promptShown);
    check(controller);
    QCOMPARE(shown.count(), 0);
    QVERIFY(!QFile::exists(file));
    modal.reject();
    controller.presentPending();
    QCOMPARE(shown.count(), 1);
    QVERIFY(QFile::exists(file));
  }
  void settingsModalIntegration_data()
  {
    QTest::addColumn<QString>("result");
    QTest::newRow("newer-long-notes") << "newer";
    QTest::newRow("current") << "current";
    QTest::newRow("failure-retry") << "failure";
    QTest::newRow("close-in-flight") << "abandon";
    QTest::newRow("existing-result") << "existing-result";
    QTest::newRow("existing-inflight") << "existing-inflight";
  }
  void settingsModalIntegration()
  {
    QFETCH(QString, result);
    QTemporaryDir temporary;
    const QString file = temporary.filePath("reminders.ini");
    Core core;
    GuiMain window;
    window.loadStyle();
    window.resize(1000, 720);
    window.show();
    auto* controller = new ShadowUpdateController("1.1.2", &window, source(), file);
    window.setUpdateController(controller);
    m_delay = 100;
    if(result == "current")
      m_body = manifest("1.1.2");
    else if(result == "failure")
      m_body = "invalid";
    else
    {
      QJsonObject object = QJsonDocument::fromJson(manifest()).object();
      object["notes"] = QString(2000, QChar(0x66f4));
      m_body = QJsonDocument(object).toJson();
    }
    const int initialRequests = m_requests;
    if(result == "existing-result")
    {
      const QByteArray response = m_body;
      m_body = "invalid";
      check(*controller, true);
      QVERIFY(controller->dialog()->isVisible());
      m_body = response;
    }
    if(result == "existing-inflight")
    {
      controller->checkManually();
      QVERIFY(controller->isChecking());
      QVERIFY(controller->dialog()->isVisible());
    }
    QSignalSpy shown(controller, &ShadowUpdateController::promptShown);
    QSignalSpy finished(controller, &ShadowUpdateController::checkFinished);
    const int opened = m_opened;
    bool entered = false;
    QTimer::singleShot(50, &window, [&]() {
      auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
      QVERIFY(dialog);
      entered = true;
      QTimer::singleShot(6000, dialog, &QDialog::reject);
      QCOMPARE(dialog->windowTitle(), QStringLiteral("影聊设置"));
      qInfo() << "UPDATE_UI" << result << QApplication::platformName()
        << "DPR" << dialog->devicePixelRatioF() << "dialog" << dialog->size()
        << "available" << dialog->screen()->availableGeometry().size();
      QVERIFY(dialog->frameGeometry().height() <= dialog->screen()->availableGeometry().height());
      auto* checkButton = dialog->findChild<QPushButton*>("checkUpdateFromSettings");
      auto* download = dialog->findChild<QPushButton*>("downloadUpdateFromSettings");
      auto* status = dialog->findChild<QLabel*>("ShadowUpdateSettingsStatus");
      auto* notes = dialog->findChild<QPlainTextEdit*>("ShadowUpdateSettingsNotes");
      auto* scroll = dialog->findChild<QScrollArea*>();
      QVERIFY(checkButton && download && status && notes && scroll);
      if(result == "existing-inflight")
      {
        QVERIFY(!checkButton->isEnabled());
        QVERIFY(controller->dialog() && !controller->dialog()->isVisible());
      }
      else
      {
        QVERIFY(checkButton->isEnabled());
        scroll->ensureWidgetVisible(checkButton);
        QTest::mouseClick(checkButton, Qt::LeftButton);
      }
      QVERIFY(controller->isChecking());
      QVERIFY(status->text().contains(QStringLiteral("正在检查")));
      QVERIFY(!checkButton->isEnabled());
      QVERIFY(!download->isVisible());
      QVERIFY(!controller->dialog() || !controller->dialog()->isVisible());
      if(result == "abandon")
      {
        dialog->reject();
        return;
      }
      QTRY_COMPARE(finished.count(), 1);
      QCOMPARE(m_requests, initialRequests + (result == "existing-result" ? 2 : 1));
      QVERIFY(checkButton->isEnabled());
      QCOMPARE(QApplication::activeModalWidget(), dialog);
      QVERIFY(!controller->dialog() || !controller->dialog()->isVisible());
      if(result == "failure")
      {
        QVERIFY(status->text().contains(QStringLiteral("暂时无法检查")));
        QVERIFY(!status->text().contains(QStringLiteral("已是最新")));
        QVERIFY(!download->isVisible());
        QVERIFY(!QFile::exists(file));
        m_body = manifest();
        scroll->ensureWidgetVisible(checkButton);
        QTest::mouseClick(checkButton, Qt::LeftButton);
        QTRY_COMPARE(finished.count(), 2);
      }
      if(result == "current")
      {
        QVERIFY(status->text().contains(QStringLiteral("当前已是最新版本")));
        QVERIFY(!download->isVisible());
        QVERIFY(!notes->isVisible());
        QVERIFY(!QFile::exists(file));
      }
      else
      {
        QVERIFY(status->text().contains(QStringLiteral("1.1.3")));
        QVERIFY(download->isVisible() && download->isEnabled());
        QVERIFY(notes->isVisible() && notes->height() <= 100);
        QVERIFY(status->height() < 100);
        scroll->ensureWidgetVisible(download);
        QApplication::processEvents();
        const QRect downloadRect(download->mapTo(scroll->viewport(), QPoint()), download->size());
        QVERIFY(scroll->viewport()->rect().contains(downloadRect));
        QTest::mouseClick(download, Qt::LeftButton);
        QCOMPARE(m_opened, opened + 1);
        QVERIFY(QFile::exists(file));
      }
      scroll->ensureWidgetVisible(status);
      QApplication::processEvents();
      const QRect statusRect(status->mapTo(scroll->viewport(), QPoint()), status->size());
      QVERIFY(scroll->viewport()->rect().contains(statusRect));
      const QString folder = qEnvironmentVariable("SHADOW_UPDATE_SCREENSHOT_DIR");
      if(!folder.isEmpty())
        QVERIFY(dialog->grab().save(QDir(folder).filePath("settings-modal-" + result + ".png")));
      auto* buttons = dialog->findChild<QDialogButtonBox*>();
      QVERIFY(buttons);
      scroll->ensureWidgetVisible(buttons->button(QDialogButtonBox::Cancel));
      QApplication::processEvents();
      QTest::mouseClick(buttons->button(QDialogButtonBox::Cancel), Qt::LeftButton);
    });
    QVERIFY(QMetaObject::invokeMethod(&window, "showShadowSettings", Qt::DirectConnection));
    QVERIFY(entered);
    QCOMPARE(shown.count(), 0);
    if(result == "abandon")
    {
      QTRY_COMPARE(finished.count(), 1);
      QVERIFY(!QFile::exists(file));
    }
    controller->shutdown();
  }
  void parentWindowDestruction()
  {
    QTemporaryDir temporary;
    QPointer<QWidget> window = new QWidget;
    window->show();
    QPointer<ShadowUpdateController> controller = new ShadowUpdateController(
      "1.1.2", window, source(), temporary.filePath("reminders.ini"));
    check(*controller, true);
    QPointer<QDialog> dialog = controller->dialog();
    QVERIFY(dialog && dialog->isVisible());
    delete window.data();
    QVERIFY(window.isNull());
    QVERIFY(controller.isNull());
    QVERIFY(dialog.isNull());
    QApplication::processEvents();
  }
  void manualClosePersists_data()
  {
    QTest::addColumn<QString>("action");
    QTest::newRow("close") << "closeUpdate";
    QTest::newRow("escape") << "escape";
    QTest::newRow("download") << "downloadUpdate";
  }
  void manualClosePersists()
  {
    QFETCH(QString, action);
    QTemporaryDir temporary;
    const QString file = temporary.filePath("reminders.ini");
    QDate day(2026, 10, 29);
    {
      ShadowUpdateController manual("1.1.2", nullptr, source(), file);
      manual.setDateProvider([&]() { return day; });
      check(manual, true);
      if(action == "escape")
        QTest::keyClick(manual.dialog(), Qt::Key_Escape);
      else
        manual.dialog()->findChild<QPushButton*>(action)->click();
      QVERIFY(!manual.dialog()->isVisible());
    }
    ShadowUpdateController restarted("1.1.2", nullptr, source(), file);
    restarted.setDateProvider([&]() { return day; });
    QSignalSpy shown(&restarted, &ShadowUpdateController::promptShown);
    check(restarted);
    QCOMPARE(shown.count(), 0);
    day = day.addDays(1);
    check(restarted);
    QCOMPARE(shown.count(), 1);
  }
  void persistenceChild()
  {
    const QString file=qEnvironmentVariable("SHADOW_REMINDER_TEST_FILE");
    if(file.isEmpty()) QSKIP("Only invoked by the cross-process persistence test");
    ShadowUpdateController controller("1.1.2",nullptr,source(),file);
    const QDate day=QDate::fromString(qEnvironmentVariable("SHADOW_REMINDER_TEST_DAY"),Qt::ISODate);
    controller.setDateProvider([day]() { return day; });
    QSignalSpy shown(&controller,&ShadowUpdateController::promptShown);
    check(controller);
    QCOMPARE(shown.count(),qEnvironmentVariableIntValue("SHADOW_REMINDER_EXPECT_PROMPT"));
    if(controller.dialog()) controller.dialog()->close();
  }
  void persistenceAcrossProcesses()
  {
    QTemporaryDir temporary;
    auto child=[&](const QString& date,int expected) {
      QProcess process;
      QProcessEnvironment environment=QProcessEnvironment::systemEnvironment();
      environment.insert("SHADOW_REMINDER_TEST_FILE",temporary.filePath("reminders.ini"));
      environment.insert("SHADOW_REMINDER_TEST_DAY",date);
      environment.insert("SHADOW_REMINDER_EXPECT_PROMPT",QString::number(expected));
      process.setProcessEnvironment(environment);
      process.start(QCoreApplication::applicationFilePath(),QStringList()<<"persistenceChild"<<"-o"<<"-,txt");
      QVERIFY(process.waitForFinished(10000));
      const QByteArray output=process.readAllStandardOutput()+process.readAllStandardError();
      QVERIFY2(process.exitCode()==0,output.constData());
    };
    child("2026-10-29",1);
    child("2026-10-29",0);
    child("2026-10-28",0);
    child("2026-10-30",1);
  }
  void cleanupTestCase() { QDesktopServices::unsetUrlHandler("http"); QDesktopServices::unsetUrlHandler("https"); }
};
int main(int argc, char** argv)
{
  QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
  QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
  BeeApplication application(argc, argv);
  application.setFont(ShadowStyle::applicationFont());
  ShadowDialogChrome::install(&application);
  UpdateSuggestionTest test;
  return QTest::qExec(&test, argc, argv);
}
#include "update_gate_test.moc"
