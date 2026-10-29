#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QTimer>
#include <cstdio>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QJsonObject result;
    result.insert("supportsSsl", QSslSocket::supportsSsl());
    result.insert("buildVersion", QSslSocket::sslLibraryBuildVersionString());
    result.insert("runtimeVersion", QSslSocket::sslLibraryVersionString());
    if (!QSslSocket::supportsSsl() || argc < 2) {
        std::puts(QJsonDocument(result).toJson(QJsonDocument::Compact).constData());
        return QSslSocket::supportsSsl() ? 0 : 1;
    }
    QSslConfiguration configuration = QSslConfiguration::defaultConfiguration();
    configuration.setPeerVerifyMode(QSslSocket::VerifyPeer);
    if (argc > 2) {
        QList<QSslCertificate> authorities = configuration.caCertificates();
        authorities.append(QSslCertificate::fromPath(QString::fromLocal8Bit(argv[2])));
        configuration.setCaCertificates(authorities);
    }
    QNetworkAccessManager network;
    QNetworkRequest request(QUrl(QString::fromLocal8Bit(argv[1])));
    request.setSslConfiguration(configuration);
    QNetworkReply* reply = network.get(request);
    QJsonArray errors;
    QObject::connect(reply, &QNetworkReply::sslErrors, [&](const QList<QSslError>& failures) {
        for (const QSslError& failure : failures)
            errors.append(failure.errorString());
    });
    bool timedOut = false;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, [&]() {
        timedOut = true;
        reply->abort();
    });
    timeout.start(15000);
    QObject::connect(reply, &QNetworkReply::finished, &app, &QCoreApplication::quit);
    app.exec();
    result.insert("certificateErrors", errors);
    result.insert("timedOut", timedOut);
    result.insert("httpStatus", reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
    result.insert("networkError", static_cast<int>(reply->error()));
    result.insert("httpsSucceeded", !timedOut && errors.isEmpty() && reply->error() == QNetworkReply::NoError);
    std::puts(QJsonDocument(result).toJson(QJsonDocument::Compact).constData());
    return result.value("httpsSucceeded").toBool() ? 0 : 1;
}
