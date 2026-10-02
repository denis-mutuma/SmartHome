#include "session_controller.h"
#include "token_store.h"

#include <QHostAddress>
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

QTEST_MAIN(SessionControllerTest)
#include "tst_session_controller.moc"