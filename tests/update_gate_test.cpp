#include "ShadowUpdateController.h"
#include "Updater.h"
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
    object["installerUrl"] = "/fixture.exe";
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
    QCOMPARE(m_openedUrl, source().resolved(QUrl("/tools/shadowchat")));
    QVERIFY(work.isEnabled());
    QVERIFY(!controller.dialog()->isVisible());
    check(controller, true);
    controller.dialog()->findChild<QPushButton*>("closeUpdate")->click();
    QVERIFY(work.isEnabled());
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
  void cleanupTestCase() { QDesktopServices::unsetUrlHandler("http"); }
};
QTEST_MAIN(UpdateSuggestionTest)
#include "update_gate_test.moc"
