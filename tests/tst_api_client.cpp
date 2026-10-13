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
    void authRequests_data();
    void authRequests();
    void emptyCityIsSentAsNull();
    void cancelledRequestsDoNotEmitResults();
    void concurrentUnauthorizedRequestsRetryIndependently();
    void deviceInsert_data();
    void deviceInsert();
    void rejectsThermometerInsert();
};

void ApiClientTest::authRequests_data()
{
    QTest::addColumn<int>("requestKind");
    QTest::newRow("sign-in") << 0;
    QTest::newRow("sign-up") << 1;
    QTest::newRow("refresh") << 2;
    QTest::newRow("logout") << 3;
}

void ApiClientTest::authRequests()
{
    QFETCH(int, requestKind);

    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    QByteArray request;
    bool requestComplete = false;
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
            requestComplete = true;
            socket->write(QByteArrayLiteral(
                "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\n{}"));
        });
    });

    ApiClient client(QStringLiteral("http://127.0.0.1:%1/").arg(server.serverPort()), QStringLiteral("public-anon-key"));
    QSignalSpy started(&client, &ApiClient::requestStarted);
    QSignalSpy completed(&client, &ApiClient::completed);
    QVERIFY(started.isValid());
    QVERIFY(completed.isValid());
    switch (requestKind) {
    case 0:
        client.signIn(QStringLiteral("person@example.com"), QStringLiteral("correct-horse"));
        break;
    case 1:
        client.signUp(QStringLiteral("person@example.com"), QStringLiteral("correct-horse"), QStringLiteral("Amina"));
        break;
    case 2:
        client.refresh(QStringLiteral("refresh-secret"));
        break;
    case 3:
        client.setAccessToken(QStringLiteral("access-secret"));
        client.logOut();
        break;
    }

    QTRY_VERIFY_WITH_TIMEOUT(requestComplete, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(completed.count(), 1, 5000);
    QCOMPARE(started.count(), 1);
    const QStringList operations{QStringLiteral("login"), QStringLiteral("signup"),
        QStringLiteral("refresh"), QStringLiteral("logout")};
    const QString operation = operations.at(requestKind);
    QCOMPARE(started.at(0).at(0).toString(), operation);
    QCOMPARE(completed.at(0).at(0).toString(), operation);
    QCOMPARE(completed.at(0).at(1).toULongLong(), started.at(0).at(1).toULongLong());
    QCOMPARE(completed.at(0).at(2).toInt(), 200);
    QCOMPARE(completed.at(0).at(3).toByteArray(), QByteArrayLiteral("{}"));

    const qsizetype headerEnd = request.indexOf(QByteArrayLiteral("\r\n\r\n"));
    const QList<QByteArray> paths{
        QByteArrayLiteral("POST /auth/v1/token?grant_type=password HTTP/1.1\r\n"),
        QByteArrayLiteral("POST /auth/v1/signup HTTP/1.1\r\n"),
        QByteArrayLiteral("POST /auth/v1/token?grant_type=refresh_token HTTP/1.1\r\n"),
        QByteArrayLiteral("POST /auth/v1/logout HTTP/1.1\r\n")};
    const QByteArray expectedPath = paths.at(requestKind);
    QVERIFY(request.startsWith(expectedPath));
    const QByteArray headers = request.left(headerEnd).toLower();
    QVERIFY(headers.contains(QByteArrayLiteral("\r\ncontent-type: application/json")));
    QVERIFY(headers.contains(QByteArrayLiteral("\r\napikey: public-anon-key")));
    if (requestKind == 3) {
        QVERIFY(headers.contains(QByteArrayLiteral("\r\nauthorization: bearer access-secret")));
    } else {
        QVERIFY(!headers.contains(QByteArrayLiteral("\r\nauthorization:")));
    }
    const QJsonObject body = QJsonDocument::fromJson(request.mid(headerEnd + 4)).object();
    if (requestKind <= 1) {
        QCOMPARE(body.value(QStringLiteral("email")).toString(), QStringLiteral("person@example.com"));
        QCOMPARE(body.value(QStringLiteral("password")).toString(), QStringLiteral("correct-horse"));
    }
    if (requestKind == 1) {
        QCOMPARE(body.value(QStringLiteral("data")).toObject().value(QStringLiteral("first_name")).toString(),
            QStringLiteral("Amina"));
    } else if (requestKind == 2) {
        QCOMPARE(body.value(QStringLiteral("refresh_token")).toString(), QStringLiteral("refresh-secret"));
    } else if (requestKind == 3) {
        QCOMPARE(body, QJsonObject{});
    }
}

void ApiClientTest::emptyCityIsSentAsNull()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    QByteArray request;
    bool requestComplete = false;
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
            requestComplete = true;
            socket->write(QByteArrayLiteral(
                "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\n{}"));
        });
    });

    ApiClient client(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()),
        QStringLiteral("public-anon-key"));
    client.setAccessToken(QStringLiteral("access-token"));
    QSignalSpy completed(&client, &ApiClient::completed);
    QVERIFY(completed.isValid());

    client.updateProfile(QStringLiteral("123e4567-e89b-12d3-a456-426614174000"), QStringLiteral("Amina"), QString());

    QTRY_VERIFY_WITH_TIMEOUT(requestComplete, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(completed.count(), 1, 5000);
    QVERIFY(request.startsWith(QByteArrayLiteral("PATCH /rest/v1/profiles?id=eq.")));
    const QJsonObject body = QJsonDocument::fromJson(
        request.mid(request.indexOf(QByteArrayLiteral("\r\n\r\n")) + 4)).object();
    QCOMPARE(body.value(QStringLiteral("first_name")).toString(), QStringLiteral("Amina"));
    QCOMPARE(body.value(QStringLiteral("city")).type(), QJsonValue::Null);
}

void ApiClientTest::cancelledRequestsDoNotEmitResults()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    ApiClient client(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()),
        QStringLiteral("public-anon-key"));
    client.setAccessToken(QStringLiteral("access-token"));
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
    QTRY_VERIFY_WITH_TIMEOUT(acceptedSocket != nullptr, 5000);

    QSignalSpy disconnected(acceptedSocket, &QTcpSocket::disconnected);
    QVERIFY(disconnected.isValid());
    client.cancelPendingRequests();

    QTRY_COMPARE_WITH_TIMEOUT(disconnected.count(), 1, 5000);
    QCOMPARE(completed.count(), 0);
    QCOMPARE(failed.count(), 0);
}

void ApiClientTest::concurrentUnauthorizedRequestsRetryIndependently()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    ApiClient client(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()),
        QStringLiteral("public-anon-key"));
    client.setAccessToken(QStringLiteral("old-access"));
    QSignalSpy authenticationRequired(&client, &ApiClient::authenticationRequired);
    QSignalSpy completed(&client, &ApiClient::completed);
    QSignalSpy failed(&client, &ApiClient::failed);
    QVERIFY(authenticationRequired.isValid());
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
    QTRY_COMPARE_WITH_TIMEOUT(authenticationRequired.count(), 2, 5000);
    QCOMPARE(failed.count(), 0);

    QMap<QString, quint64> requestIds;
    for (const QList<QVariant>& arguments : authenticationRequired) {
        requestIds.insert(arguments.at(0).toString(), arguments.at(1).toULongLong());
    }
    QCOMPARE(requestIds.size(), 2);
    QVERIFY(requestIds.contains(QStringLiteral("profile")));
    QVERIFY(requestIds.contains(QStringLiteral("rooms")));
    QVERIFY(requestIds.value(QStringLiteral("profile")) != requestIds.value(QStringLiteral("rooms")));
    QVERIFY(!client.queueRetry(requestIds.value(QStringLiteral("profile"))));
    QVERIFY(!client.queueRetry(requestIds.value(QStringLiteral("rooms"))));

    client.setAccessToken(QStringLiteral("fresh-access"));
    client.retryQueuedRequests();
    QTRY_COMPARE_WITH_TIMEOUT(completed.count(), 2, 5000);
    QCOMPARE(failed.count(), 0);
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

void ApiClientTest::deviceInsert_data()
{
    QTest::addColumn<QString>("kind");
    QTest::newRow("light") << QStringLiteral("light");
    QTest::newRow("plug") << QStringLiteral("plug");
}

void ApiClientTest::deviceInsert()
{
    QFETCH(QString, kind);

    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    QByteArray request;
    bool requestComplete = false;
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
            requestComplete = true;
            socket->write(QByteArrayLiteral("HTTP/1.1 201 Created\r\nContent-Length: 2\r\nConnection: close\r\n\r\n[]"));
        });
    });

    const QString roomId = QStringLiteral("123e4567-e89b-12d3-a456-426614174000");
    ApiClient client(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()), QStringLiteral("public-anon-key"));
    client.setAccessToken(QStringLiteral("access-token"));
    QSignalSpy completed(&client, &ApiClient::completed);
    QVERIFY(completed.isValid());
    client.insertDevice(roomId, QStringLiteral("Probe"), kind, 4);

    QTRY_VERIFY_WITH_TIMEOUT(requestComplete, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(completed.count(), 1, 5000);
    QVERIFY(request.startsWith(QByteArrayLiteral(
        "POST /rest/v1/devices?select=id,room_id,name,kind,is_on,celsius,reading_at,position HTTP/1.1\r\n")));
    QVERIFY(request.left(request.indexOf(QByteArrayLiteral("\r\n\r\n"))).toLower()
        .contains(QByteArrayLiteral("\r\nauthorization: bearer access-token")));

    const QJsonObject body = QJsonDocument::fromJson(
        request.mid(request.indexOf(QByteArrayLiteral("\r\n\r\n")) + 4)).object();
    QCOMPARE(body.value(QStringLiteral("room_id")).toString(), roomId);
    QCOMPARE(body.value(QStringLiteral("kind")).toString(), kind);
    QCOMPARE(body.value(QStringLiteral("position")).toInt(), 4);
    QCOMPARE(body.value(QStringLiteral("is_on")).toBool(), false);
    QVERIFY(body.value(QStringLiteral("celsius")).isNull());
    QVERIFY(body.value(QStringLiteral("reading_at")).isNull());
}

void ApiClientTest::rejectsThermometerInsert()
{
    ApiClient client(QStringLiteral("http://127.0.0.1:1"), QStringLiteral("public-anon-key"));
    QSignalSpy failed(&client, &ApiClient::failed);
    QVERIFY(failed.isValid());

    client.insertDevice(QStringLiteral("123e4567-e89b-12d3-a456-426614174000"),
        QStringLiteral("Probe"), QStringLiteral("thermometer"), 0);

    QCOMPARE(failed.count(), 1);
    QCOMPARE(failed.first().at(0).toString(), QStringLiteral("device-insert"));
    QCOMPARE(failed.first().at(1).toULongLong(), quint64(0));
}

QTEST_GUILESS_MAIN(ApiClientTest)
#include "tst_api_client.moc"