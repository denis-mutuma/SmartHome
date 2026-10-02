#include "api_client.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

class ApiClientTest : public QObject
{
    Q_OBJECT

private slots:
    void emptyCityIsSentAsNull();
    void cancelledRequestsDoNotEmitResults();
    void concurrentUnauthorizedRequestsRetryIndependently();
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

void ApiClientTest::cancelledRequestsDoNotEmitResults()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    ApiClient client(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()), QStringLiteral("anon"));
    client.setAccessToken(QStringLiteral("access"));
    QSignalSpy completed(&client, &ApiClient::completed);
    QSignalSpy failed(&client, &ApiClient::failed);
    QVERIFY(completed.isValid());
    QVERIFY(failed.isValid());

    QTcpSocket* acceptedSocket = nullptr;
    connect(&server, &QTcpServer::newConnection, &server, [&]() {
        acceptedSocket = server.nextPendingConnection();
    });
    client.updateProfile(QStringLiteral("123e4567-e89b-12d3-a456-426614174000"),
        QStringLiteral("Amina"), QStringLiteral("Nairobi"));
    QTRY_VERIFY(acceptedSocket != nullptr);

    QSignalSpy disconnected(acceptedSocket, &QTcpSocket::disconnected);
    QVERIFY(disconnected.isValid());
    client.cancelPendingRequests();

    QTRY_COMPARE(disconnected.count(), 1);
    QCOMPARE(completed.count(), 0);
    QCOMPARE(failed.count(), 0);
}

void ApiClientTest::concurrentUnauthorizedRequestsRetryIndependently()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    ApiClient client(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()), QStringLiteral("anon"));
    client.setAccessToken(QStringLiteral("old-access"));
    QSignalSpy completed(&client, &ApiClient::completed);
    QSignalSpy failed(&client, &ApiClient::failed);
    QVERIFY(completed.isValid());
    QVERIFY(failed.isValid());

    QList<QByteArray> requests;
    connect(&server, &QTcpServer::newConnection, &server, [&]() {
        QTcpSocket* socket = server.nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, socket, [&, socket]() {
            QByteArray request = socket->property("request").toByteArray();
            request.append(socket->readAll());
            socket->setProperty("request", request);
            if (request.indexOf(QByteArrayLiteral("\r\n\r\n")) < 0 || socket->property("responded").toBool()) {
                return;
            }
            socket->setProperty("responded", true);
            requests.append(request);
            const QByteArray response = requests.size() <= 2
                ? QByteArrayLiteral("HTTP/1.1 401 Unauthorized\r\nContent-Length: 0\r\nConnection: close\r\n\r\n")
                : QByteArrayLiteral("HTTP/1.1 204 No Content\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
            socket->write(response);
        });
    });

    client.fetchProfile();
    client.fetchRooms();
    QTRY_COMPARE(failed.count(), 2);

    QMap<QString, quint64> requestIds;
    for (const QList<QVariant>& arguments : failed) {
        QCOMPARE(arguments.at(2).toInt(), 401);
        requestIds.insert(arguments.at(0).toString(), arguments.at(1).toULongLong());
    }
    QVERIFY(requestIds.contains(QStringLiteral("profile")));
    QVERIFY(requestIds.contains(QStringLiteral("rooms")));
    QVERIFY(client.queueRetry(requestIds.value(QStringLiteral("profile"))));
    QVERIFY(client.queueRetry(requestIds.value(QStringLiteral("rooms"))));
    QVERIFY(!client.queueRetry(requestIds.value(QStringLiteral("profile"))));

    client.setAccessToken(QStringLiteral("fresh-access"));
    client.retryQueuedRequests();
    QTRY_COMPARE(completed.count(), 2);
    QCOMPARE(requests.size(), 4);

    QMap<QByteArray, int> oldTokenRequests;
    QMap<QByteArray, int> freshTokenRequests;
    for (const QByteArray& request : requests) {
        const qsizetype lineEnd = request.indexOf(QByteArrayLiteral("\r\n"));
        const QList<QByteArray> line = request.left(lineEnd).split(' ');
        QVERIFY(line.size() >= 2);
        const QByteArray path = line.at(1).left(line.at(1).indexOf('?'));
        const QByteArray headers = request.left(request.indexOf(QByteArrayLiteral("\r\n\r\n"))).toLower();
        if (headers.contains(QByteArrayLiteral("authorization: bearer old-access"))) {
            ++oldTokenRequests[path];
        } else if (headers.contains(QByteArrayLiteral("authorization: bearer fresh-access"))) {
            ++freshTokenRequests[path];
        }
    }
    QCOMPARE(oldTokenRequests.value(QByteArrayLiteral("/rest/v1/profiles")), 1);
    QCOMPARE(oldTokenRequests.value(QByteArrayLiteral("/rest/v1/rooms")), 1);
    QCOMPARE(freshTokenRequests.value(QByteArrayLiteral("/rest/v1/profiles")), 1);
    QCOMPARE(freshTokenRequests.value(QByteArrayLiteral("/rest/v1/rooms")), 1);
}

QTEST_GUILESS_MAIN(ApiClientTest)
#include "tst_api_client.moc"