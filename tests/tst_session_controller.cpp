#include "session_controller.h"
#include "token_store.h"

#include <QHostAddress>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>

class SessionControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void refreshNetworkFailureKeepsSavedToken();
    void rejectedRefreshTokenIsCleared();
    void tokenPersistenceFailureKeepsSessionWithWarning();
    void emptyMutationResultIsNotSuccess();
    void manualLoginSupersedesStartupRefresh();
    void signOutWarnsWhenSavedSessionCannotBeRemoved();
    void olderRoomsResponseCannotOverwriteNewerState();
};

void SessionControllerTest::refreshNetworkFailureKeepsSavedToken()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString tokenPath = directory.filePath(QStringLiteral("session.bin"));
    const QString refreshToken = QStringLiteral("refresh-token-value");
    QVERIFY(saveRefreshToken(refreshToken, tokenPath));

    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    connect(&server, &QTcpServer::newConnection, &server, [&server]() {
        if (QTcpSocket* socket = server.nextPendingConnection()) {
            socket->abort();
        }
    });

    SessionController controller(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()),
        QStringLiteral("anon"), tokenPath);
    QTRY_VERIFY(!controller.statusMessage().isEmpty());
    QVERIFY(!controller.signedIn());
    QCOMPARE(loadRefreshToken(tokenPath), refreshToken);
}

void SessionControllerTest::rejectedRefreshTokenIsCleared()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString tokenPath = directory.filePath(QStringLiteral("session.bin"));
    QVERIFY(saveRefreshToken(QStringLiteral("rejected-refresh-token"), tokenPath));

    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    const QByteArray responseBody = QByteArrayLiteral(
        "{\"code\":\"refresh_token_not_found\",\"msg\":\"invalid\"}");
    connect(&server, &QTcpServer::newConnection, &server, [&server, responseBody]() {
        QTcpSocket* socket = server.nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, socket, [socket, responseBody]() {
            socket->readAll();
            if (socket->property("replied").toBool()) {
                return;
            }
            socket->setProperty("replied", true);
            socket->write(QByteArrayLiteral("HTTP/1.1 400 Bad Request\r\nContent-Length: ")
                + QByteArray::number(responseBody.size()) + QByteArrayLiteral("\r\nConnection: close\r\n\r\n")
                + responseBody);
        });
    });

    SessionController controller(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()),
        QStringLiteral("anon"), tokenPath);
    QTRY_VERIFY(!controller.statusMessage().isEmpty());
    QVERIFY(!controller.signedIn());
    QVERIFY(loadRefreshToken(tokenPath).isEmpty());
}

void SessionControllerTest::tokenPersistenceFailureKeepsSessionWithWarning()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString blockerPath = directory.filePath(QStringLiteral("not-a-directory"));
    QFile blocker(blockerPath);
    QVERIFY(blocker.open(QIODevice::WriteOnly));
    blocker.close();
    const QString tokenPath = blockerPath + QStringLiteral("/session.bin");

    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    const QByteArray sessionBody = QByteArrayLiteral(
        "{\"access_token\":\"access\",\"refresh_token\":\"refresh\","
        "\"user\":{\"id\":\"123e4567-e89b-12d3-a456-426614174000\"}}");
    connect(&server, &QTcpServer::newConnection, &server, [&server, sessionBody]() {
        QTcpSocket* socket = server.nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, socket, [socket, sessionBody]() {
            const QByteArray request = socket->readAll();
            if (request.startsWith(QByteArrayLiteral("POST /auth/v1/token"))) {
                socket->write(QByteArrayLiteral("HTTP/1.1 200 OK\r\nContent-Length: ")
                    + QByteArray::number(sessionBody.size()) + QByteArrayLiteral("\r\nConnection: close\r\n\r\n")
                    + sessionBody);
            } else {
                socket->write(QByteArrayLiteral("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\n[]"));
            }
        });
    });

    SessionController controller(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()),
        QStringLiteral("anon"), tokenPath);
    QVERIFY(controller.signIn(QStringLiteral("person@example.com"), QStringLiteral("password123")));
    QTRY_VERIFY(controller.signedIn());
    QVERIFY(controller.statusMessage().contains(QStringLiteral("couldn't be saved")));
    QVERIFY(!QFile::exists(tokenPath));
}

void SessionControllerTest::emptyMutationResultIsNotSuccess()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    const QByteArray sessionBody = QByteArrayLiteral(
        "{\"access_token\":\"access\",\"refresh_token\":\"refresh\","
        "\"user\":{\"id\":\"123e4567-e89b-12d3-a456-426614174000\"}}");
    const QByteArray profileBody = QByteArrayLiteral(
        "[{\"id\":\"123e4567-e89b-12d3-a456-426614174000\",\"first_name\":\"Amina\",\"city\":null}]");
    int roomInsertCount = 0;
    connect(&server, &QTcpServer::newConnection, &server, [&server, sessionBody, profileBody, &roomInsertCount]() {
        QTcpSocket* socket = server.nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, socket, [socket, sessionBody, profileBody, &roomInsertCount]() {
            QByteArray request = socket->property("request").toByteArray();
            request.append(socket->readAll());
            socket->setProperty("request", request);
            const qsizetype headerEnd = request.indexOf(QByteArrayLiteral("\r\n\r\n"));
            if (headerEnd < 0 || socket->property("replied").toBool()) {
                return;
            }
            const QList<QByteArray> requestLine = request.left(headerEnd).split('\n').first().trimmed().split(' ');
            if (requestLine.size() < 2) {
                return;
            }
            const QByteArray method = requestLine.at(0);
            const QByteArray path = requestLine.at(1);
            QByteArray body = QByteArrayLiteral("[]");
            int status = 200;
            if (path.startsWith(QByteArrayLiteral("/auth/v1/token?"))) {
                body = sessionBody;
            } else if (path.startsWith(QByteArrayLiteral("/rest/v1/profiles?"))) {
                body = profileBody;
            } else if (path.startsWith(QByteArrayLiteral("/rest/v1/rooms?")) && method == "POST") {
                ++roomInsertCount;
                status = 201;
            }
            socket->setProperty("replied", true);
            socket->write(QByteArrayLiteral("HTTP/1.1 ") + QByteArray::number(status)
                + QByteArrayLiteral(" OK\r\nContent-Length: ") + QByteArray::number(body.size())
                + QByteArrayLiteral("\r\nConnection: close\r\n\r\n") + body);
        });
    });

    SessionController controller(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()),
        QStringLiteral("anon"), directory.filePath(QStringLiteral("session.bin")));
    QSignalSpy mutationFinished(&controller, &SessionController::mutationFinished);
    QVERIFY(mutationFinished.isValid());
    QVERIFY(controller.signIn(QStringLiteral("person@example.com"), QStringLiteral("password123")));
    QTRY_VERIFY(controller.signedIn());
    QVERIFY(controller.createRoom(QStringLiteral("Office")));
    QTRY_COMPARE(mutationFinished.count(), 1);
    QCOMPARE(mutationFinished.at(0).at(0).toString(), QStringLiteral("room-insert"));
    QCOMPARE(mutationFinished.at(0).at(1).toBool(), false);
    QCOMPARE(roomInsertCount, 1);
}

void SessionControllerTest::manualLoginSupersedesStartupRefresh()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString tokenPath = directory.filePath(QStringLiteral("session.bin"));
    QVERIFY(saveRefreshToken(QStringLiteral("old-refresh-token"), tokenPath));

    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    const QByteArray newSession = QByteArrayLiteral(
        "{\"access_token\":\"new-access\",\"refresh_token\":\"new-refresh\","
        "\"user\":{\"id\":\"123e4567-e89b-12d3-a456-426614174000\"}}");
    QTcpSocket* pendingRefresh = nullptr;
    int refreshCount = 0;
    int loginCount = 0;
    connect(&server, &QTcpServer::newConnection, &server, [&]() {
        QTcpSocket* socket = server.nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, socket, [&, socket]() {
            QByteArray request = socket->property("request").toByteArray();
            request.append(socket->readAll());
            socket->setProperty("request", request);
            const qsizetype headerEnd = request.indexOf(QByteArrayLiteral("\r\n\r\n"));
            if (headerEnd < 0 || socket->property("handled").toBool()) {
                return;
            }
            const QByteArray requestLine = request.left(request.indexOf(QByteArrayLiteral("\r\n")));
            if (requestLine.contains(QByteArrayLiteral("grant_type=refresh_token"))) {
                ++refreshCount;
                pendingRefresh = socket;
                socket->setProperty("handled", true);
                return;
            }
            if (requestLine.contains(QByteArrayLiteral("grant_type=password"))) {
                ++loginCount;
                socket->setProperty("handled", true);
                socket->write(QByteArrayLiteral("HTTP/1.1 200 OK\r\nContent-Length: ")
                    + QByteArray::number(newSession.size()) + QByteArrayLiteral("\r\nConnection: close\r\n\r\n")
                    + newSession);
                return;
            }
            const QByteArray body = requestLine.contains(QByteArrayLiteral("/rest/v1/profiles"))
                ? QByteArrayLiteral("[{\"id\":\"123e4567-e89b-12d3-a456-426614174000\",\"first_name\":\"New\",\"city\":null}]")
                : QByteArrayLiteral("[]");
            socket->setProperty("handled", true);
            socket->write(QByteArrayLiteral("HTTP/1.1 200 OK\r\nContent-Length: ")
                + QByteArray::number(body.size()) + QByteArrayLiteral("\r\nConnection: close\r\n\r\n") + body);
        });
    });

    SessionController controller(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()),
        QStringLiteral("anon"), tokenPath);
    QTRY_COMPARE(refreshCount, 1);
    QVERIFY(controller.signIn(QStringLiteral("new@example.com"), QStringLiteral("password123")));
    QTRY_VERIFY(controller.signedIn());
    QTRY_COMPARE(loginCount, 1);
    QTRY_VERIFY(pendingRefresh->state() == QAbstractSocket::UnconnectedState);
    QCOMPARE(loadRefreshToken(tokenPath), QStringLiteral("new-refresh"));
}

void SessionControllerTest::signOutWarnsWhenSavedSessionCannotBeRemoved()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString tokenPath = directory.filePath(QStringLiteral("session.bin"));

    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    const QByteArray sessionBody = QByteArrayLiteral(
        "{\"access_token\":\"access\",\"refresh_token\":\"refresh\","
        "\"user\":{\"id\":\"123e4567-e89b-12d3-a456-426614174000\"}}");
    connect(&server, &QTcpServer::newConnection, &server, [&server, sessionBody]() {
        QTcpSocket* socket = server.nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, socket, [socket, sessionBody]() {
            const QByteArray request = socket->readAll();
            const QByteArray body = request.startsWith(QByteArrayLiteral("POST /auth/v1/token"))
                ? sessionBody : QByteArrayLiteral("[]");
            socket->write(QByteArrayLiteral("HTTP/1.1 200 OK\r\nContent-Length: ")
                + QByteArray::number(body.size()) + QByteArrayLiteral("\r\nConnection: close\r\n\r\n") + body);
        });
    });

    SessionController controller(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()),
        QStringLiteral("anon"), tokenPath);
    QVERIFY(controller.signIn(QStringLiteral("person@example.com"), QStringLiteral("password123")));
    QTRY_VERIFY(controller.signedIn());
    QVERIFY(!loadRefreshToken(tokenPath).isEmpty());

    QVERIFY(QFile::remove(tokenPath));
    QVERIFY(QDir().mkdir(tokenPath));
    controller.signOut();

    QVERIFY(!controller.signedIn());
    QVERIFY(controller.statusMessage().contains(QStringLiteral("saved session")));
    QVERIFY(QFileInfo(tokenPath).isDir());
}

void SessionControllerTest::olderRoomsResponseCannotOverwriteNewerState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    const QByteArray jwtPayload = QByteArrayLiteral(
        R"({"sub":"123e4567-e89b-12d3-a456-426614174000","exp":2000000000})");
    const QString accessToken = QStringLiteral("aaa.")
        + QString::fromLatin1(jwtPayload.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals))
        + QStringLiteral(".bbb");
    const QByteArray sessionBody = QStringLiteral(
        R"({"access_token":"%1","refresh_token":"refresh","user":{"id":"123e4567-e89b-12d3-a456-426614174000"}})")
        .arg(accessToken).toUtf8();
    const QByteArray profileBody = QByteArrayLiteral(
        R"([{"id":"123e4567-e89b-12d3-a456-426614174000","first_name":"Amina","city":null}])");
    const QByteArray staleRooms = QByteArrayLiteral(
        R"([{"id":"123e4567-e89b-12d3-a456-426614174001","name":"Stale","position":0,"devices":[]}])");
    const QByteArray currentRooms = QByteArrayLiteral(
        R"([{"id":"123e4567-e89b-12d3-a456-426614174002","name":"Current","position":0,"devices":[]}])");
    QTcpSocket* firstRoomsSocket = nullptr;
    int roomsRequests = 0;

    connect(&server, &QTcpServer::newConnection, &server, [&]() {
        QTcpSocket* socket = server.nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, socket,
            [&, socket]() {
                QByteArray request = socket->property("request").toByteArray();
                request.append(socket->readAll());
                socket->setProperty("request", request);
                const qsizetype headerEnd = request.indexOf(QByteArrayLiteral("\r\n\r\n"));
                if (headerEnd < 0 || socket->property("handled").toBool()) {
                    return;
                }
                const QByteArray requestLine = request.left(request.indexOf(QByteArrayLiteral("\r\n")));
                socket->setProperty("handled", true);
                QByteArray body = QByteArrayLiteral("[]");
                if (requestLine.contains(QByteArrayLiteral("POST /auth/v1/token"))) {
                    body = sessionBody;
                } else if (requestLine.contains(QByteArrayLiteral("/rest/v1/profiles"))) {
                    body = profileBody;
                } else if (requestLine.contains(QByteArrayLiteral("/rest/v1/rooms"))) {
                    ++roomsRequests;
                    if (roomsRequests == 1) {
                        firstRoomsSocket = socket;
                        return;
                    }
                    body = currentRooms;
                }
                socket->write(QByteArrayLiteral("HTTP/1.1 200 OK\r\nContent-Length: ")
                    + QByteArray::number(body.size()) + QByteArrayLiteral("\r\nConnection: close\r\n\r\n") + body);
            });
    });

    SessionController controller(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()),
        QStringLiteral("anon"), directory.filePath(QStringLiteral("session.bin")));
    QSignalSpy roomsChanged(&controller, &SessionController::roomsChanged);
    QVERIFY(roomsChanged.isValid());
    QVERIFY(controller.signIn(QStringLiteral("person@example.com"), QStringLiteral("password123")));
    QTRY_COMPARE(roomsRequests, 1);
    QVERIFY(firstRoomsSocket != nullptr);

    controller.reload();
    QTRY_COMPARE(roomsRequests, 2);
    QTRY_COMPARE(controller.rooms().size(), 1);
    QCOMPARE(controller.rooms().first().toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Current"));
    QCOMPARE(roomsChanged.count(), 1);

    QSignalSpy staleResponseFinished(firstRoomsSocket, &QTcpSocket::disconnected);
    QVERIFY(staleResponseFinished.isValid());
    firstRoomsSocket->write(QByteArrayLiteral("HTTP/1.1 200 OK\r\nContent-Length: ")
        + QByteArray::number(staleRooms.size()) + QByteArrayLiteral("\r\nConnection: close\r\n\r\n") + staleRooms);
    QTRY_COMPARE(staleResponseFinished.count(), 1);
    QCOMPARE(roomsChanged.count(), 1);
    QCOMPARE(controller.rooms().first().toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Current"));
}

QTEST_MAIN(SessionControllerTest)
#include "tst_session_controller.moc"