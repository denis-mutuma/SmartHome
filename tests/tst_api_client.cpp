#include "api_client.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

class ApiClientTest : public QObject
{
    Q_OBJECT

private slots:
    void emptyCityIsSentAsNull();
};

void ApiClientTest::emptyCityIsSentAsNull()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    ApiClient client(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()), QStringLiteral("anon"));
    client.setAccessToken(QStringLiteral("access"));
    QSignalSpy completed(&client, &ApiClient::completed);
    QVERIFY(completed.isValid());

    QByteArray request;
    QByteArray requestBody;
    connect(&server, &QTcpServer::newConnection, &server, [&]() {
        QTcpSocket* socket = server.nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, socket, [&, socket]() {
            request.append(socket->readAll());
            const qsizetype headerEnd = request.indexOf(QByteArrayLiteral("\r\n\r\n"));
            if (headerEnd < 0) {
                return;
            }
            const QByteArray headers = request.left(headerEnd).toLower();
            const qsizetype lengthStart = headers.indexOf(QByteArrayLiteral("content-length:"));
            if (lengthStart < 0) {
                return;
            }
            const qsizetype valueStart = lengthStart + qsizetype(sizeof("content-length:") - 1);
            const qsizetype lineEnd = headers.indexOf(QByteArrayLiteral("\r\n"), valueStart);
            bool validLength = false;
            const qint64 contentLength = headers.mid(valueStart, lineEnd - valueStart).trimmed().toLongLong(&validLength);
            if (!validLength || request.size() < headerEnd + 4 + contentLength) {
                return;
            }
            requestBody = request.mid(headerEnd + 4, contentLength);
            socket->write(QByteArrayLiteral(
                "HTTP/1.1 204 No Content\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"));
        });
    });

    client.updateProfile(QStringLiteral("123e4567-e89b-12d3-a456-426614174000"),
        QStringLiteral("Amina"), QString());

    QTRY_VERIFY(!requestBody.isEmpty());
    const QJsonDocument payload = QJsonDocument::fromJson(requestBody);
    QVERIFY(payload.isObject());
    QCOMPARE(payload.object().value(QStringLiteral("first_name")).toString(), QStringLiteral("Amina"));
    QCOMPARE(payload.object().value(QStringLiteral("city")).type(), QJsonValue::Null);
    QTRY_COMPARE(completed.count(), 1);
}

QTEST_GUILESS_MAIN(ApiClientTest)
#include "tst_api_client.moc"