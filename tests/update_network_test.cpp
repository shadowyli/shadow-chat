#include "Updater.h"
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
  else passed=passed && updater.downloadUrl()==url.resolved(QUrl("/tools/shadowchat")).toString();
  if(fixture)passed=passed && fixture->requests==1 && fixture->lastRequest.startsWith("GET /updates/shadowchat.json HTTP/1.") && !fixture->lastRequest.toLower().contains("cookie:") && !fixture->lastRequest.toLower().contains("authorization:");
  std::printf("%s %s completions=%d version=%s\n",passed?"PASS":"FAIL",name,completions,qPrintable(updater.versionAvailable()));
  return passed;
}

int main(int argc,char** argv)
{
  QCoreApplication app(argc,argv);
  bool passed=true;
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
