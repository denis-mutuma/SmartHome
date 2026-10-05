#include "session_controller.h"

#include "token_store.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

namespace {

struct AuthServerState
{
    QByteArray sessionBody;
    QByteArray refreshBody;
    QByteArray loginRequest;
    QByteArray signupRequest;
    QByteArray refreshRequest;
    QByteArray logoutRequest;
    QByteArray profileRequest;
    QByteArray profileUpdateRequest;
    QByteArray profileBody = QByteArrayLiteral(
        R"([{"id":"123e4567-e89b-12d3-a456-426614174000","first_name":"Amina","city":null}])");
    QByteArray updatedProfileBody = QByteArrayLiteral(
        R"([{"id":"123e4567-e89b-12d3-a456-426614174000","first_name":"Amina K","city":"Nairobi"}])");
    int refreshRequests = 0;
    int refreshStatus = 200;
};

void startAuthServer(QTcpServer& server, AuthServerState& state)
{
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&server, &state]() {
        QTcpSocket* socket = server.nextPendingConnection();
        auto request = std::make_shared<QByteArray>();
        QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket, request, &state]() {
            if (socket->property("responded").toBool()) {
                return;
            }
            request->append(socket->readAll());
            const qsizetype headerEnd = request->indexOf(QByteArrayLiteral("\r\n\r\n"));
            if (headerEnd < 0) {
                return;
            }
            const qsizetype lineEnd = request->indexOf(QByteArrayLiteral("\r\n"));
            const QByteArray requestLine = request->left(lineEnd);
            const QByteArray headers = request->left(headerEnd).toLower();
            const qsizetype lengthStart = headers.indexOf(QByteArrayLiteral("content-length:"));
            if (lengthStart >= 0) {
                const qsizetype valueStart = lengthStart + qsizetype(sizeof("content-length:") - 1);
                const qsizetype lengthEnd = headers.indexOf(QByteArrayLiteral("\r\n"), valueStart);
                bool validLength = false;
                const qint64 contentLength = headers.mid(valueStart, lengthEnd - valueStart)
                                                 .trimmed().toLongLong(&validLength);
                if (!validLength || request->size() < headerEnd + 4 + contentLength) {
                    return;
                }
            }

            const QByteArray path = requestLine.split(' ').value(1);
            int status = 200;
            QByteArray reason = QByteArrayLiteral("OK");
            QByteArray payload = state.sessionBody;
            if (path == QByteArrayLiteral("/auth/v1/token?grant_type=password")) {
                state.loginRequest = *request;
            } else if (path == QByteArrayLiteral("/auth/v1/signup")) {
                state.signupRequest = *request;
            } else if (path == QByteArrayLiteral("/auth/v1/token?grant_type=refresh_token")) {
                ++state.refreshRequests;
                state.refreshRequest = *request;
                status = state.refreshStatus;
                reason = status == 401 ? QByteArrayLiteral("Unauthorized") : QByteArrayLiteral("OK");
                payload = state.refreshBody.isEmpty() ? state.sessionBody : state.refreshBody;
            } else if (path == QByteArrayLiteral("/auth/v1/logout")) {
                state.logoutRequest = *request;
                status = 204;
                reason = QByteArrayLiteral("No Content");
                payload.clear();
            } else if (path == QByteArrayLiteral("/rest/v1/profiles?select=id,first_name,city")) {
                state.profileRequest = *request;
                payload = state.profileBody;
            } else if (requestLine.startsWith(QByteArrayLiteral("PATCH /rest/v1/profiles?id=eq."))) {
                state.profileUpdateRequest = *request;
                payload = state.updatedProfileBody;
            } else {
                return;
            }
            socket->setProperty("responded", true);
            socket->write(QByteArrayLiteral("HTTP/1.1 ") + QByteArray::number(status) + ' ' + reason
                + QByteArrayLiteral("\r\nContent-Type: application/json\r\nContent-Length: ")
                + QByteArray::number(payload.size()) + QByteArrayLiteral("\r\nConnection: close\r\n\r\n")
                + payload);
        });
    });
}

QByteArray sessionResponse(QString accessToken = QStringLiteral("access-token"),
    QString refreshToken = QStringLiteral("refresh-token"), int expiresIn = 3600)
{
    return QJsonDocument(QJsonObject{
        {QStringLiteral("access_token"), std::move(accessToken)},
        {QStringLiteral("refresh_token"), std::move(refreshToken)},
        {QStringLiteral("expires_in"), expiresIn},
        {QStringLiteral("user"), QJsonObject{
            {QStringLiteral("id"), QStringLiteral("123e4567-e89b-12d3-a456-426614174000")},
            {QStringLiteral("email"), QStringLiteral("person@example.com")}}}})
        .toJson(QJsonDocument::Compact);
}

QJsonObject requestBody(const QByteArray& request)
{
    const qsizetype headerEnd = request.indexOf(QByteArrayLiteral("\r\n\r\n"));
    return QJsonDocument::fromJson(request.mid(headerEnd + 4)).object();
}

} // namespace

class SessionControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void signInPersistsRefreshTokenAndSignOutClearsIt();
    void registerSendsNormalizedAccountDetails();
    void rejectsInvalidCredentials();
    void restoresSessionWithRotatedRefreshToken();
    void clearsRejectedStoredSession();
    void refreshesNearExpirySessionOnce();
    void loadsAndSavesProfileSettings();
};

void SessionControllerTest::signInPersistsRefreshTokenAndSignOutClearsIt()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString tokenPath = directory.filePath(QStringLiteral("refresh-token.bin"));

    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    AuthServerState state{sessionResponse()};
    startAuthServer(server, state);

    SessionController controller(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()),
        QStringLiteral("public-anon-key"), tokenPath);
    QSignalSpy signedIn(&controller, &SessionController::signedInChanged);
    QVERIFY(signedIn.isValid());
    QVERIFY(controller.signIn(QStringLiteral(" Person@Example.com "), QStringLiteral("correct-horse")));
    QTRY_VERIFY_WITH_TIMEOUT(controller.signedIn(), 5000);

    QCOMPARE(signedIn.count(), 1);
    QCOMPARE(controller.email(), QStringLiteral("person@example.com"));
    QCOMPARE(loadRefreshToken(tokenPath), QStringLiteral("refresh-token"));
    QCOMPARE(requestBody(state.loginRequest).value(QStringLiteral("email")).toString(),
        QStringLiteral("person@example.com"));
    QCOMPARE(requestBody(state.loginRequest).value(QStringLiteral("password")).toString(),
        QStringLiteral("correct-horse"));

    controller.signOut();
    QVERIFY(!controller.signedIn());
    QVERIFY(loadRefreshToken(tokenPath).isEmpty());
    QTRY_VERIFY_WITH_TIMEOUT(!state.logoutRequest.isEmpty(), 5000);
    QVERIFY(state.logoutRequest.toLower().contains(QByteArrayLiteral("authorization: bearer access-token")));
}

void SessionControllerTest::registerSendsNormalizedAccountDetails()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    AuthServerState state{sessionResponse()};
    startAuthServer(server, state);

    SessionController controller(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()),
        QStringLiteral("public-anon-key"), directory.filePath(QStringLiteral("refresh-token.bin")));
    QVERIFY(controller.registerAccount(QStringLiteral(" Amina "), QStringLiteral(" Person@Example.com "),
        QStringLiteral("correct-horse")));
    QTRY_VERIFY_WITH_TIMEOUT(controller.signedIn(), 5000);

    const QJsonObject body = requestBody(state.signupRequest);
    QCOMPARE(body.value(QStringLiteral("email")).toString(), QStringLiteral("person@example.com"));
    QCOMPARE(body.value(QStringLiteral("password")).toString(), QStringLiteral("correct-horse"));
    QCOMPARE(body.value(QStringLiteral("data")).toObject().value(QStringLiteral("first_name")).toString(),
        QStringLiteral("Amina"));
    QCOMPARE(loadRefreshToken(directory.filePath(QStringLiteral("refresh-token.bin"))),
        QStringLiteral("refresh-token"));
}

void SessionControllerTest::rejectsInvalidCredentials()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SessionController controller(QStringLiteral("http://127.0.0.1"), QStringLiteral("public-anon-key"),
        directory.filePath(QStringLiteral("refresh-token.bin")));

    QVERIFY(!controller.signIn(QStringLiteral("not-an-email"), QStringLiteral("correct-horse")));
    QVERIFY(!controller.statusMessage().isEmpty());
    QVERIFY(!controller.signIn(QStringLiteral("person@example.com"), QStringLiteral("short")));
    QVERIFY(!controller.registerAccount(QString(), QStringLiteral("person@example.com"),
        QStringLiteral("correct-horse")));
    QVERIFY(!controller.signedIn());
    QVERIFY(loadRefreshToken(directory.filePath(QStringLiteral("refresh-token.bin"))).isEmpty());
}

void SessionControllerTest::restoresSessionWithRotatedRefreshToken()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString tokenPath = directory.filePath(QStringLiteral("refresh-token.bin"));
    QVERIFY(saveRefreshToken(QStringLiteral("old-refresh-token"), tokenPath));

    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    AuthServerState state{sessionResponse()};
    state.refreshBody = sessionResponse(QStringLiteral("new-access-token"),
        QStringLiteral("rotated-refresh-token"));
    startAuthServer(server, state);

    SessionController controller(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()),
        QStringLiteral("public-anon-key"), tokenPath);
    QTRY_VERIFY_WITH_TIMEOUT(controller.signedIn(), 5000);

    QCOMPARE(controller.email(), QStringLiteral("person@example.com"));
    QCOMPARE(loadRefreshToken(tokenPath), QStringLiteral("rotated-refresh-token"));
    QCOMPARE(state.refreshRequests, 1);
    QCOMPARE(requestBody(state.refreshRequest).value(QStringLiteral("refresh_token")).toString(),
        QStringLiteral("old-refresh-token"));
}

void SessionControllerTest::clearsRejectedStoredSession()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString tokenPath = directory.filePath(QStringLiteral("refresh-token.bin"));
    QVERIFY(saveRefreshToken(QStringLiteral("expired-refresh-token"), tokenPath));

    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    AuthServerState state{QByteArrayLiteral(R"({"message":"invalid refresh token"})")};
    state.refreshStatus = 401;
    startAuthServer(server, state);

    SessionController controller(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()),
        QStringLiteral("public-anon-key"), tokenPath);
    QTRY_COMPARE_WITH_TIMEOUT(state.refreshRequests, 1, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(loadRefreshToken(tokenPath).isEmpty(), 5000);
    QVERIFY(!controller.signedIn());
}

void SessionControllerTest::refreshesNearExpirySessionOnce()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString tokenPath = directory.filePath(QStringLiteral("refresh-token.bin"));

    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    AuthServerState state{sessionResponse(QStringLiteral("short-lived-access-token"),
        QStringLiteral("old-refresh-token"), 30)};
    state.refreshBody = sessionResponse(QStringLiteral("new-access-token"),
        QStringLiteral("rotated-refresh-token"));
    startAuthServer(server, state);

    SessionController controller(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()),
        QStringLiteral("public-anon-key"), tokenPath);
    QVERIFY(controller.signIn(QStringLiteral("person@example.com"), QStringLiteral("correct-horse")));
    QTRY_VERIFY_WITH_TIMEOUT(controller.signedIn(), 5000);
    QVERIFY(QMetaObject::invokeMethod(&controller, "refreshIfNeeded", Qt::DirectConnection));
    QVERIFY(QMetaObject::invokeMethod(&controller, "refreshIfNeeded", Qt::DirectConnection));
    QTRY_COMPARE_WITH_TIMEOUT(state.refreshRequests, 1, 5000);

    QCOMPARE(loadRefreshToken(tokenPath), QStringLiteral("rotated-refresh-token"));
    QCOMPARE(requestBody(state.refreshRequest).value(QStringLiteral("refresh_token")).toString(),
        QStringLiteral("old-refresh-token"));
}

void SessionControllerTest::loadsAndSavesProfileSettings()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    AuthServerState state{sessionResponse()};
    startAuthServer(server, state);

    SessionController controller(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()),
        QStringLiteral("public-anon-key"), directory.filePath(QStringLiteral("refresh-token.bin")));
    QVERIFY(controller.signIn(QStringLiteral("person@example.com"), QStringLiteral("correct-horse")));
    QTRY_COMPARE_WITH_TIMEOUT(controller.firstName(), QStringLiteral("Amina"), 5000);
    QCOMPARE(controller.city(), QString());
    QVERIFY(state.profileRequest.left(state.profileRequest.indexOf(QByteArrayLiteral("\r\n\r\n")))
        .toLower().contains(QByteArrayLiteral("\r\nauthorization: bearer access-token")));

    QVERIFY(!controller.saveSettings(QString(), QStringLiteral("Nairobi")));
    QVERIFY(!controller.statusMessage().isEmpty());
    QVERIFY(state.profileUpdateRequest.isEmpty());
    QVERIFY(!controller.saveSettings(QStringLiteral("Amina"), QString(81, QLatin1Char('x'))));
    QVERIFY(state.profileUpdateRequest.isEmpty());

    QVERIFY(controller.saveSettings(QStringLiteral(" Amina K "), QStringLiteral(" Nairobi ")));
    QTRY_COMPARE_WITH_TIMEOUT(controller.firstName(), QStringLiteral("Amina K"), 5000);
    QCOMPARE(controller.city(), QStringLiteral("Nairobi"));

    const qsizetype headerEnd = state.profileUpdateRequest.indexOf(QByteArrayLiteral("\r\n\r\n"));
    QVERIFY(state.profileUpdateRequest.left(headerEnd).toLower()
        .contains(QByteArrayLiteral("\r\nauthorization: bearer access-token")));
    const QJsonObject body = requestBody(state.profileUpdateRequest);
    QCOMPARE(body.value(QStringLiteral("first_name")).toString(), QStringLiteral("Amina K"));
    QCOMPARE(body.value(QStringLiteral("city")).toString(), QStringLiteral("Nairobi"));
}

QTEST_MAIN(SessionControllerTest)
#include "tst_session_controller.moc"
