#include "Updater.h"
#include "ShadowGitHubRelease.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QEventLoop>
#include <QTimer>
#include <QSslSocket>
#include <QSslCertificate>
#include <QSslKey>
#include <QFile>
#include <cstdio>

static QByteArray manifest(const QByteArray& version="9.0.0",bool published=true)
{
  return "{\"schemaVersion\":1,\"product\":\"shadowchat\",\"channel\":\"test\",\"mandatory\":true,\"published\":"+QByteArray(published?"true":"false")+",\"version\":\""+version+"\",\"downloadPage\":\"/tools/shadowchat\",\"installerUrl\":\"/downloads/fixture.exe\",\"sha256\":\""+QByteArray(64,'a')+"\"}";
}

struct HttpFixture
{
  QTcpServer server;
  QByteArray status,body,extraHeaders,lastRequest;
  int requests=0;
  bool silent=false;
  explicit HttpFixture(const QByteArray& response,const QByteArray& responseStatus="200 OK") : status(responseStatus),body(response)
  {
    if(!server.listen(QHostAddress::LocalHost,0))qFatal("Cannot start loopback fixture");
    QObject::connect(&server,&QTcpServer::newConnection,[this](){
      while(server.hasPendingConnections()) {
        QTcpSocket* socket=server.nextPendingConnection();
        QObject::connect(socket,&QTcpSocket::readyRead,[this,socket](){
          QByteArray request=socket->property("request").toByteArray()+socket->readAll(); socket->setProperty("request",request);
          if(!request.contains("\r\n\r\n") || socket->property("responded").toBool())return;
          socket->setProperty("responded",true); ++requests; lastRequest=request;
          if(silent)return;
          socket->write("HTTP/1.1 "+status+"\r\nContent-Type: application/json\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\n"+extraHeaders+"\r\n"+body);
          socket->disconnectFromHost();
        });
      }
    });
  }
  QUrl url() const {return QUrl("http://127.0.0.1:"+QString::number(server.serverPort())+"/updates/shadowchat.json");}
};

class UntrustedTlsFixture : public QTcpServer
{
  QSslCertificate m_certificate;
  QSslKey m_key;
public:
  explicit UntrustedTlsFixture(const QString& folder)
  {
    QFile certificate(folder+"/server.pem"),key(folder+"/synthetic-server-key.pem");
    if(!certificate.open(QIODevice::ReadOnly) || !key.open(QIODevice::ReadOnly))
      qFatal("Synthetic TLS fixture unavailable");
    m_certificate=QSslCertificate(certificate.readAll());
    m_key=QSslKey(key.readAll(),QSsl::Rsa);
    if(m_certificate.isNull() || m_key.isNull() || !listen(QHostAddress::LocalHost,0))
      qFatal("Cannot initialize synthetic TLS fixture");
  }
protected:
  void incomingConnection(qintptr descriptor) override
  {
    QSslSocket* socket=new QSslSocket(this);
    socket->setLocalCertificate(m_certificate);
    socket->setPrivateKey(m_key);
    socket->setPeerVerifyMode(QSslSocket::VerifyNone);
    socket->setSocketDescriptor(descriptor);
    connect(socket,&QSslSocket::disconnected,socket,&QObject::deleteLater);
    socket->startServerEncryption();
  }
};

static bool run(const char* name,const QUrl& url,HttpFixture* fixture,const QString& expectedVersion=QString(),bool expectedNew=false,
                const QString& channel=QStringLiteral("test"),const QString& expectedNews=QString())
{
  Updater updater(nullptr,url,channel);
  updater.setNetworkTimeoutsForTesting(1500, 2000);
  QEventLoop loop;
  int completions=0;
  bool timedOut=false;
  QObject::connect(&updater,&Updater::jobCompleted,[&](){++completions;loop.quit();});
  QTimer watchdog; watchdog.setSingleShot(true);
  QObject::connect(&watchdog,&QTimer::timeout,[&](){timedOut=true;loop.quit();}); watchdog.start(12000);
  updater.checkForNewVersion(); if(!completions)loop.exec();
  const bool newVersion=!updater.versionAvailable().isEmpty() && QVersionNumber::compare(QVersionNumber::fromString(updater.versionAvailable()),QVersionNumber(1,1,0))>0;
  bool passed=!timedOut && completions==1 && updater.versionAvailable()==expectedVersion && updater.news()==expectedNews && newVersion==expectedNew;
  passed=passed && updater.manifestValid()==!expectedVersion.isEmpty();
  passed=passed && updater.hasNewerVersion("1.1.0")==expectedNew;
  passed=passed && updater.errorString().isEmpty()==!expectedVersion.isEmpty();
  if(expectedVersion.isEmpty())passed=passed && updater.downloadUrl().isEmpty();
  else passed=passed && updater.downloadUrl()==url.resolved(QUrl("/downloads/fixture.exe")).toString();
  if(fixture)passed=passed && fixture->requests==1 && fixture->lastRequest.startsWith("GET /updates/shadowchat.json HTTP/1.") && !fixture->lastRequest.toLower().contains("cookie:") && !fixture->lastRequest.toLower().contains("authorization:");
  std::printf("%s %s completions=%d version=%s\n",passed?"PASS":"FAIL",name,completions,qPrintable(updater.versionAvailable()));
  return passed;
}

struct ReleaseFixture
{
  struct Response
  {
    QByteArray body;
    QByteArray status = "200 OK";
    QByteArray headers;
    int delay = 0;
    bool silent = false;
  };
  QTcpServer server;
  QMap<QByteArray, Response> routes;
  QList<QByteArray> paths;
  QList<QByteArray> requests;

  static QByteArray apiPath() { return "/repos/shadowyli/shadow-chat/releases"; }
  static QByteArray feedPath() { return "/shadowyli/shadow-chat/releases.atom"; }
  static QByteArray assetPath(const QByteArray& version, const QByteArray& name)
  {
    return "/downloads/v" + version + "-test/" + name;
  }
  static QByteArray filename(const QByteArray& version = "9.1.0", const QByteArray& environment = "local")
  {
    return "ShadowChat-" + version + "-" + environment + "-setup.exe";
  }
  static QJsonObject metadata(const QString& version = "9.1.0", const QString& environment = "local")
  {
    return QJsonObject{
      {"product", "ShadowChat"}, {"version", version}, {"channel", "test"},
      {"releaseEnvironment", environment}, {"sourceTag", "v" + version + "-test"},
      {"sourceCommit", QString(40, 'b')},
      {"binaryBuildIdentity", QJsonObject{{"version", version}, {"channel", "test"}, {"environment", environment}}},
      {"installer", QJsonObject{{"file", QString::fromLatin1(filename(version.toLatin1(), environment.toLatin1()))},
        {"bytes", 1000}, {"sha256", QString(64, 'a')}}}
    };
  }
  static QByteArray feed(const QList<QByteArray>& versions)
  {
    QByteArray body = "<feed xmlns=\"http://www.w3.org/2005/Atom\">";
    for(const QByteArray& version : versions)
    {
      const QByteArray tag = "v" + version + "-test";
      body += "<entry><id>tag:github.com,2008:Repository/123/" + tag +
        "</id><link rel=\"alternate\" href=\"https://github.com/shadowyli/shadow-chat/releases/tag/" +
        tag + "\"/><content type=\"html\">&lt;li&gt;Public update&lt;/li&gt;</content></entry>";
    }
    return body + "</feed>";
  }
  void install(const QByteArray& version = "9.1.0", const QByteArray& environment = "local")
  {
    routes[assetPath(version, "build-info.json")].body =
      QJsonDocument(metadata(QString::fromLatin1(version), QString::fromLatin1(environment))).toJson();
    routes[assetPath(version, "SHA256SUMS.txt")].body =
      QByteArray(64, 'a') + "  " + filename(version, environment) + "\n";
  }
  ReleaseFixture()
  {
    if(!server.listen(QHostAddress::LocalHost))
      qFatal("Cannot start release fixture");
    routes[apiPath()].status = "403 Forbidden";
    routes[feedPath()].body = feed({"9.1.0", "9.0.0"});
    install();
    install("9.0.0");
    QObject::connect(&server, &QTcpServer::newConnection, &server, [this]() {
      while(server.hasPendingConnections())
      {
        QTcpSocket* socket = server.nextPendingConnection();
        QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        QObject::connect(socket, &QTcpSocket::readyRead, &server, [this, socket]() {
          const QByteArray request = socket->property("request").toByteArray() + socket->readAll();
          socket->setProperty("request", request);
          if(!request.contains("\r\n\r\n") || socket->property("sent").toBool())
            return;
          socket->setProperty("sent", true);
          const QByteArray path = request.split(' ').value(1);
          paths.append(path);
          requests.append(request);
          Response response;
          response.status = "404 Not Found";
          if(routes.contains(path))
            response = routes.value(path);
          if(response.silent)
            return;
          QTimer::singleShot(response.delay, socket, [socket, response]() {
            if(socket->state() != QAbstractSocket::ConnectedState)
              return;
            socket->write("HTTP/1.1 " + response.status +
              "\r\nContent-Length: " + QByteArray::number(response.body.size()) +
              "\r\nConnection: close\r\n" + response.headers + "\r\n" + response.body);
            socket->disconnectFromHost();
          });
        });
      }
    });
  }
  QUrl source() const
  {
    return QUrl(QStringLiteral("http://127.0.0.1:%1%2").arg(server.serverPort())
      .arg(QString::fromLatin1(apiPath())));
  }
};

static bool waitCheck(Updater& updater, int* completions)
{
  QEventLoop loop;
  bool watchdogExpired = false;
  const auto connection = QObject::connect(&updater, &Updater::jobCompleted, &loop, [&]() {
    ++*completions;
    loop.quit();
  });
  QTimer watchdog;
  watchdog.setSingleShot(true);
  QObject::connect(&watchdog, &QTimer::timeout, &loop, [&]() { watchdogExpired = true; loop.quit(); });
  watchdog.start(4000);
  updater.checkForNewVersion();
  for(int attempt = 0; attempt < 5; ++attempt)
    if(updater.isChecking())
      updater.checkForNewVersion();
  if(updater.isChecking())
    loop.exec();
  if(!watchdogExpired)
  {
    QTimer::singleShot(100, &loop, &QEventLoop::quit);
    loop.exec();
  }
  QObject::disconnect(connection);
  return !watchdogExpired && !updater.isChecking();
}

static bool fallbackCase(const char* name, const std::function<void(ReleaseFixture&)>& configure,
  const QString& expected = QStringLiteral("9.1.0"), bool repeatBackoff = false)
{
  ReleaseFixture fixture;
  configure(fixture);
  Updater updater(nullptr, fixture.source(), "test");
  updater.setNetworkTimeoutsForTesting(100, 1200);
  int completions = 0;
  bool passed = waitCheck(updater, &completions) && completions == 1;
  passed = passed && updater.manifestValid() == !expected.isEmpty() && updater.versionAvailable() == expected;
  passed = passed && updater.errorString().isEmpty() == !expected.isEmpty();
  passed = passed && updater.hasNewerVersion("9.0.0") == (!expected.isEmpty() && expected != "9.0.0");
  if(expected.isEmpty())
    passed = passed && updater.downloadUrl().isEmpty() && updater.news().isEmpty() &&
      !fixture.paths.contains(ReleaseFixture::assetPath("9.0.0", "build-info.json"));
  if(repeatBackoff)
  {
    passed = passed && updater.isApiBackedOff();
    passed = waitCheck(updater, &completions) && passed && completions == 2;
    passed = passed && fixture.paths.count(ReleaseFixture::apiPath()) == 1 &&
      fixture.paths.count(ReleaseFixture::feedPath()) == 2;
  }
  for(const QByteArray& request : fixture.requests)
    passed = passed && !request.toLower().contains("authorization:") &&
      !request.toLower().contains("cookie:") && !request.toLower().contains("token");
  std::printf("%s fallback %s completions=%d requests=%d version=%s error=%s\n",
    passed ? "PASS" : "FAIL", name, completions, fixture.paths.size(),
    qPrintable(updater.versionAvailable()), qPrintable(updater.errorString()));
  return passed;
}

static bool fallbackTests()
{
  bool passed = true;
  const QString invalid;
  passed = fallbackCase("403", [](ReleaseFixture&) {}) && passed;
  passed = fallbackCase("429", [](ReleaseFixture& fixture) {
    fixture.routes[ReleaseFixture::apiPath()].status = "429 Too Many Requests";
  }) && passed;
  passed = fallbackCase("timeout", [](ReleaseFixture& fixture) {
    fixture.routes[ReleaseFixture::apiPath()].silent = true;
  }) && passed;
  passed = fallbackCase("late API result completes once", [](ReleaseFixture& fixture) {
    fixture.routes[ReleaseFixture::apiPath()].delay = 160;
  }) && passed;
  passed = fallbackCase("Retry-After skips API", [](ReleaseFixture& fixture) {
    fixture.routes[ReleaseFixture::apiPath()].headers = "Retry-After: 60\r\n";
  }, "9.1.0", true) && passed;
  passed = fallbackCase("rate reset skips API", [](ReleaseFixture& fixture) {
    fixture.routes[ReleaseFixture::apiPath()].headers = "X-RateLimit-Remaining: 0\r\nX-RateLimit-Reset: " +
      QByteArray::number(QDateTime::currentSecsSinceEpoch() + 60) + "\r\n";
  }, "9.1.0", true) && passed;
  passed = fallbackCase("API invalid JSON uses public feed", [](ReleaseFixture& fixture) {
    fixture.routes[ReleaseFixture::apiPath()].status = "200 OK";
    fixture.routes[ReleaseFixture::apiPath()].body = "{";
  }) && passed;
  const QList<QPair<QString, QJsonValue>> mutations{
    {"version", "9.2.0"}, {"channel", "stable"}, {"releaseEnvironment", "dev"},
    {"sourceTag", "v9.0.0-test"}, {"sourceCommit", "invalid"}, {"product", "Other"}
  };
  for(const auto& mutation : mutations)
    passed = fallbackCase(qPrintable("bad metadata " + mutation.first + " cannot use older valid release"),
      [mutation](ReleaseFixture& fixture) {
        QJsonObject metadata = ReleaseFixture::metadata();
        metadata[mutation.first] = mutation.second;
        fixture.routes[ReleaseFixture::assetPath("9.1.0", "build-info.json")].body =
          QJsonDocument(metadata).toJson();
      }, invalid) && passed;
  const QList<QPair<QString, QJsonValue>> installerMutations{
    {"file", "ShadowChat-9.1.0-dev-setup.exe"}, {"file", "../ShadowChat-9.1.0-local-setup.exe"},
    {"bytes", 0}, {"bytes", -1}, {"bytes", 1.5}, {"bytes", "1000"}, {"bytes", true},
    {"bytes", QJsonValue()}, {"bytes", 1e50}, {"sha256", "bad"}
  };
  for(const auto& mutation : installerMutations)
    passed = fallbackCase(qPrintable("bad installer " + mutation.first + "=" +
      QString::fromUtf8(QJsonDocument(QJsonArray{mutation.second}).toJson(QJsonDocument::Compact))),
      [mutation](ReleaseFixture& fixture) {
        QJsonObject metadata = ReleaseFixture::metadata();
        QJsonObject installer = metadata["installer"].toObject();
        installer[mutation.first] = mutation.second;
        metadata["installer"] = installer;
        fixture.routes[ReleaseFixture::assetPath("9.1.0", "build-info.json")].body =
          QJsonDocument(metadata).toJson();
      }, invalid) && passed;
  passed = fallbackCase("valid other environment skipped", [](ReleaseFixture& fixture) {
    fixture.install("9.1.0", "dev");
  }, "9.0.0") && passed;
  for(const QByteArray& name : {QByteArray("build-info.json"), QByteArray("SHA256SUMS.txt")})
  {
    passed = fallbackCase(qPrintable(QString::fromLatin1(name) + " 404 cannot use older release"),
      [name](ReleaseFixture& fixture) {
        fixture.routes[ReleaseFixture::assetPath("9.1.0", name)].status = "404 Not Found";
      }, invalid) && passed;
    passed = fallbackCase(qPrintable(QString::fromLatin1(name) + " timeout"),
      [name](ReleaseFixture& fixture) {
        fixture.routes[ReleaseFixture::assetPath("9.1.0", name)].silent = true;
      }, invalid) && passed;
  }
  passed = fallbackCase("checksum mismatch", [](ReleaseFixture& fixture) {
    fixture.routes[ReleaseFixture::assetPath("9.1.0", "SHA256SUMS.txt")].body.replace(0, 64, QByteArray(64, 'b'));
  }, invalid) && passed;
  passed = fallbackCase("build-info checksum mismatch", [](ReleaseFixture& fixture) {
    fixture.routes[ReleaseFixture::assetPath("9.1.0", "SHA256SUMS.txt")].body +=
      QByteArray(64, 'b') + "  build-info.json\n";
  }, invalid) && passed;
  passed = fallbackCase("checksum missing", [](ReleaseFixture& fixture) {
    fixture.routes[ReleaseFixture::assetPath("9.1.0", "SHA256SUMS.txt")].body = QByteArray(64, 'a') + "  other.exe\n";
  }, invalid) && passed;
  passed = fallbackCase("metadata oversized", [](ReleaseFixture& fixture) {
    fixture.routes[ReleaseFixture::assetPath("9.1.0", "build-info.json")].body = QByteArray(70000, ' ');
  }, invalid) && passed;
  passed = fallbackCase("feed malformed", [](ReleaseFixture& fixture) {
    fixture.routes[ReleaseFixture::feedPath()].body = "<feed>";
  }, invalid) && passed;
  passed = fallbackCase("overflow version cannot use older release", [](ReleaseFixture& fixture) {
    fixture.routes[ReleaseFixture::feedPath()].body = ReleaseFixture::feed({"2147483648.0.0", "9.0.0"});
  }, invalid) && passed;
  const QList<QByteArray> redirects{
    "https://evil.example/metadata", "http://release-assets.githubusercontent.com/file",
    "https://release-assets.githubusercontent.com.evil.example/file",
    "https://user@objects.githubusercontent.com/file", "https://objects.githubusercontent.com:8443/file",
    "https://objects.githubusercontent.com/file#fragment", "/downloads/v9.0.0-test/build-info.json"
  };
  for(const QByteArray& target : redirects)
    passed = fallbackCase(qPrintable("untrusted redirect " + QString::fromLatin1(target)),
      [target](ReleaseFixture& fixture) {
        auto& response = fixture.routes[ReleaseFixture::assetPath("9.1.0", "build-info.json")];
        response.status = "302 Found";
        response.headers = "Location: " + target + "\r\n";
      }, invalid) && passed;
  passed = fallbackCase("feed redirect refused", [](ReleaseFixture& fixture) {
    auto& response = fixture.routes[ReleaseFixture::feedPath()];
    response.status = "302 Found";
    response.headers = "Location: https://objects.githubusercontent.com/file\r\n";
  }, invalid) && passed;
  return passed;
}

static bool destructionTests()
{
  bool passed = true;
  for(bool timeout : {false, true})
  {
    ReleaseFixture fixture;
    if(timeout)
      fixture.routes[ReleaseFixture::feedPath()].silent = true;
    QPointer<Updater> updater = new Updater(nullptr, fixture.source(), "test");
    updater->setNetworkTimeoutsForTesting(100, 150);
    QEventLoop loop;
    int completions = 0;
    bool valid = false;
    QObject::connect(updater, &Updater::jobCompleted, &loop, [&]() {
      ++completions;
      valid = updater->manifestValid();
      delete updater.data();
    });
    QTimer::singleShot(600, &loop, &QEventLoop::quit);
    updater->checkForNewVersion();
    loop.exec();
    const bool result = updater.isNull() && completions == 1 && valid != timeout;
    passed = result && passed;
    std::printf("%s fallback synchronous parent destruction timeout=%d completions=%d\n",
      result ? "PASS" : "FAIL", timeout, completions);
  }
  for(bool duringFallback : {false, true})
  {
    ReleaseFixture fixture;
    fixture.routes[duringFallback ? ReleaseFixture::feedPath() : ReleaseFixture::apiPath()].silent = true;
    QPointer<QObject> parent = new QObject;
    QPointer<Updater> updater = new Updater(parent, fixture.source(), "test");
    updater->setNetworkTimeoutsForTesting(100, 150);
    int completions = 0;
    QEventLoop loop;
    QObject::connect(updater, &Updater::jobCompleted, &loop, [&]() { ++completions; });
    updater->checkForNewVersion();
    QTimer::singleShot(50, &loop, [&]() { delete parent.data(); });
    QTimer::singleShot(400, &loop, &QEventLoop::quit);
    loop.exec();
    const bool result = parent.isNull() && updater.isNull() && completions == 0;
    passed = result && passed;
    std::printf("%s cancellation destroys updater duringFallback=%d completions=%d\n",
      result ? "PASS" : "FAIL", duringFallback, completions);
  }
  {
    ReleaseFixture fixture;
    Updater updater(nullptr, fixture.source(), "test");
    updater.setNetworkTimeoutsForTesting(100, 1000);
    int completions = 0;
    bool result = waitCheck(updater, &completions) && updater.manifestValid();
    fixture.routes[ReleaseFixture::assetPath("9.1.0", "build-info.json")].body = "{}";
    result = waitCheck(updater, &completions) && result && !updater.manifestValid() &&
      updater.versionAvailable().isEmpty() && updater.downloadUrl().isEmpty() &&
      updater.news().isEmpty() && !updater.errorString().isEmpty() && !updater.hasNewerVersion("9.0.0");
    fixture.install();
    result = waitCheck(updater, &completions) && result && updater.manifestValid() &&
      updater.hasNewerVersion("9.0.0") && updater.errorString().isEmpty() && completions == 3;
    passed = result && passed;
    std::printf("%s success-failure-retry clears stale result completions=%d\n", result ? "PASS" : "FAIL", completions);
  }
  return passed;
}

int main(int argc,char** argv)
{
  QCoreApplication app(argc,argv);
  bool passed=true;
  passed = fallbackTests() && passed;
  passed = destructionTests() && passed;
  {HttpFixture fixture(manifest()); passed=run("new published version",fixture.url(),&fixture,"9.0.0",true)&&passed;}
  {
    QJsonObject object=QJsonDocument::fromJson(manifest()).object();
    object["notes"]=QJsonArray{QStringLiteral("修复更新提醒"),QStringLiteral("保留原有资料")};
    HttpFixture fixture(QJsonDocument(object).toJson());
    passed=run("release notes array shown as readable lines",fixture.url(),&fixture,"9.0.0",true,"test",
      QStringLiteral("修复更新提醒\n保留原有资料"))&&passed;
  }
  {HttpFixture fixture(manifest()); QUrl local=fixture.url(); local.setHost("localhost"); passed=run("localhost fixture allowed",local,&fixture,"9.0.0",true)&&passed;}
  {HttpFixture fixture(manifest("1.0.0")); passed=run("old version never qualifies for reminder",fixture.url(),&fixture,"1.0.0",false)&&passed;}
  {HttpFixture fixture(manifest("1.1.0")); passed=run("current version allowed",fixture.url(),&fixture,"1.1.0",false)&&passed;}
  {HttpFixture fixture(manifest().replace("\"channel\":\"test\"","\"channel\":\"stable\"")); passed=run("stable release ignored by test client",fixture.url(),&fixture)&&passed;}
  {HttpFixture fixture(manifest()); passed=run("test release ignored by stable client",fixture.url(),&fixture,QString(),false,"stable")&&passed;}
  {HttpFixture fixture(manifest().replace("\"channel\":\"test\"","\"channel\":\"stable\"")); passed=run("stable client accepts stable release",fixture.url(),&fixture,"9.0.0",true,"stable")&&passed;}
  {HttpFixture fixture(manifest("999999999999999999999.0.0")); passed=run("overflow version rejected",fixture.url(),&fixture)&&passed;}
  {HttpFixture fixture(manifest("9.0.0",false)); passed=run("unpublished release ignored",fixture.url(),&fixture)&&passed;}
  {HttpFixture fixture("not-json"); passed=run("invalid JSON ignored",fixture.url(),&fixture)&&passed;}
  {HttpFixture fixture(manifest("9.0.0-beta")); passed=run("invalid version ignored",fixture.url(),&fixture)&&passed;}
  {HttpFixture fixture(manifest(),"503 Service Unavailable"); passed=run("HTTP failure ignored",fixture.url(),&fixture)&&passed;}
  {HttpFixture fixture(manifest(),"302 Found"); fixture.extraHeaders="Location: https://example.com/updates/shadowchat.json\r\n"; passed=run("redirect never followed",fixture.url(),&fixture)&&passed;}
  {HttpFixture fixture(QByteArray(70000,' ')+manifest()); passed=run("oversized response aborted",fixture.url(),&fixture)&&passed;}
  {HttpFixture fixture(manifest()); fixture.silent=true; passed=run("silent endpoint aborted within deadline",fixture.url(),&fixture)&&passed;}
  {HttpFixture fixture(manifest()); const QUrl url=fixture.url(); fixture.server.close(); passed=run("connection refused completes",url,nullptr)&&passed;}
  passed=run("external URL rejected before network",QUrl("http://example.com/updates/shadowchat.json"),nullptr)&&passed;
  passed=run("wrong local path rejected",QUrl("http://127.0.0.1:1/other.json"),nullptr)&&passed;
  passed=run("credentials rejected",QUrl("http://user:password@127.0.0.1:1/updates/shadowchat.json"),nullptr)&&passed;
  passed=run("query rejected",QUrl("http://127.0.0.1:1/updates/shadowchat.json?email=test"),nullptr)&&passed;
  passed=run("nonliteral loopback rejected",QUrl("http://127.1.2.3:1/updates/shadowchat.json"),nullptr)&&passed;
  passed=run("company HTTPS source cannot be injected through test constructor",
    QUrl("https://updates.example.test/updates/shadowchat.json"),nullptr)&&passed;
  const QString tlsFixture=qEnvironmentVariable("SHADOW_UPDATE_TLS_FIXTURE");
  if(!tlsFixture.isEmpty()) {
    if(!QSslSocket::supportsSsl()) {std::puts("FAIL TLS runtime unavailable");passed=false;}
    else {
      UntrustedTlsFixture fixture(tlsFixture);
      passed=run("untrusted certificate rejected without accepting manifest",
        QUrl(QString("https://127.0.0.1:%1/updates/shadowchat.json").arg(fixture.serverPort())),nullptr)&&passed;
    }
  }
  std::puts(passed?"PASS real QNetwork update client integration":"FAIL real QNetwork update client integration");
  return passed?0:1;
}
