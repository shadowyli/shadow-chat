#include "ShadowUpdateGate.h"
#include <QtTest>
#include <QtWidgets>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QDesktopServices>

class UpdateGateTest : public QObject
{
  Q_OBJECT
  QTcpServer m_server;
  QByteArray m_body;
  int m_opened = 0;
  QUrl m_openedUrl;

  QByteArray manifest(const QString& version, bool mandatory = false)
  {
    QJsonObject object;
    object["schemaVersion"] = 1;
    object["product"] = "shadowchat";
    object["published"] = true;
    object["version"] = version;
    object["mandatory"] = mandatory;
    object["downloadPage"] = "/tools/shadowchat";
    object["installerUrl"] = "/fixture.exe";
    object["sha256"] = QString(64, 'a');
    return QJsonDocument(object).toJson();
  }

  QUrl source() const
  {
    return QUrl(QStringLiteral("http://127.0.0.1:%1/updates/shadowchat.json").arg(m_server.serverPort()));
  }

  void screenshot(ShadowUpdateGate& gate, const QString& name)
  {
    const QString folder = qEnvironmentVariable("SHADOW_UPDATE_SCREENSHOT_DIR");
    if(folder.isEmpty())
      return;
    QVERIFY(QDir().mkpath(folder));
    QVERIFY(gate.grab().save(QDir(folder).filePath(name + ".png")));
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
          if(!request.contains("\r\n\r\n") || socket->property("sent").toBool())
            return;
          socket->setProperty("sent", true);
          socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
                        QByteArray::number(m_body.size()) + "\r\nConnection: close\r\n\r\n" + m_body);
          socket->disconnectFromHost();
        });
      }
    });
    QDesktopServices::setUrlHandler("http", this, "openFixture");
  }

  void openFixture(const QUrl& url)
  {
    ++m_opened;
    m_openedUrl = url;
  }

  void startupVersions_data()
  {
    QTest::addColumn<QString>("current");
    QTest::addColumn<QString>("released");
    QTest::addColumn<bool>("allowed");
    QTest::newRow("old") << "1.0.0" << "1.1.0" << false;
    QTest::newRow("current") << "1.1.0" << "1.1.0" << true;
    QTest::newRow("higher-development") << "2.0.0" << "1.1.0" << true;
    QTest::newRow("overflow") << "1.1.0" << "2147483648.0.0" << false;
  }

  void startupVersions()
  {
    QFETCH(QString, current);
    QFETCH(QString, released);
    QFETCH(bool, allowed);
    m_body = manifest(released);
    ShadowUpdateGate gate(current, nullptr, source());
    QSignalSpy accepted(&gate, &QDialog::accepted);
    gate.accept();
    gate.done(QDialog::Accepted);
    QCOMPARE(accepted.count(), 0);
    gate.checkNow();
    QTRY_VERIFY(gate.findChild<QPushButton*>("retryUpdate")->isEnabled());
    QCOMPARE(accepted.count(), allowed ? 1 : 0);
    if(!allowed)
    {
      gate.accept();
      QCOMPARE(accepted.count(), 0);
    }
  }

  void closeEscapeAndDownload()
  {
    m_body = manifest("9.0.0");
    ShadowUpdateGate gate("1.1.0", nullptr, source());
    QSignalSpy accepted(&gate, &QDialog::accepted);
    QSignalSpy exits(&gate, &ShadowUpdateGate::exitRequested);
    gate.checkNow();
    QTRY_VERIFY(gate.findChild<QPushButton*>("downloadUpdate")->isEnabled());
    screenshot(gate, "mandatory-update");
    const int opened = m_opened;
    gate.findChild<QPushButton*>("downloadUpdate")->click();
    QCOMPARE(m_opened, opened + 1);
    QCOMPARE(m_openedUrl, source().resolved(QUrl("/tools/shadowchat")));
    QCOMPARE(accepted.count(), 0);
    QTest::keyClick(&gate, Qt::Key_Escape);
    QCOMPARE(exits.count(), 1);
    QCOMPARE(accepted.count(), 0);
    gate.show();
    gate.close();
    QCOMPARE(exits.count(), 2);
    QCOMPARE(accepted.count(), 0);
  }

  void failureThenSuccessfulRetry()
  {
    m_body = "not-json";
    ShadowUpdateGate gate("1.1.0", nullptr, source());
    QSignalSpy accepted(&gate, &QDialog::accepted);
    gate.checkNow();
    QTRY_VERIFY(gate.findChild<QPushButton*>("retryUpdate")->isEnabled());
    QCOMPARE(accepted.count(), 0);
    QVERIFY(!gate.findChild<QPushButton*>("downloadUpdate")->isEnabled());
    screenshot(gate, "verification-failed");
    gate.accept();
    QCOMPARE(accepted.count(), 0);
    m_body = manifest("1.1.0");
    gate.checkNow();
    QTRY_COMPARE(accepted.count(), 1);
  }

  void runtimeFailureCannotResume()
  {
    m_body = manifest("1.1.0");
    ShadowUpdateGate gate("1.1.0", nullptr, source());
    QSignalSpy accepted(&gate, &QDialog::accepted);
    QSignalSpy blocked(&gate, &ShadowUpdateGate::runtimeBlocked);
    gate.startMonitoring();
    gate.checkNow();
    QTRY_VERIFY(gate.findChild<QPushButton*>("retryUpdate")->isEnabled());
    QCOMPARE(blocked.count(), 0);
    QCOMPARE(accepted.count(), 0);
    m_body = "not-json";
    gate.checkNow();
    QTRY_COMPARE(blocked.count(), 1);
    QVERIFY(gate.isBlocked());
    m_body = manifest("1.1.0");
    gate.checkNow();
    QTRY_VERIFY(gate.findChild<QPushButton*>("retryUpdate")->isEnabled());
    gate.accept();
    QCOMPARE(accepted.count(), 0);
    QVERIFY(gate.findChild<QLabel*>("updateStatus")->text().contains(QStringLiteral("重新启动")));
    QCOMPARE(blocked.count(), 1);
  }

  void cleanupTestCase()
  {
    QDesktopServices::unsetUrlHandler("http");
  }
};

QTEST_MAIN(UpdateGateTest)
#include "update_gate_test.moc"
