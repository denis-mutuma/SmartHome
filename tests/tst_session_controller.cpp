#include "session_controller.h"

#include "token_store.h"

#include <QHostAddress>
#include <QJsonArray>
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
    QByteArray roomsRequest;
    QByteArray roomInsertRequest;
    QByteArray roomUpdateRequest;
    QByteArray roomDeleteRequest;
    QByteArray deviceInsertRequest;
    QByteArray deviceToggleRequest;
    QByteArray deviceReadingRequest;
    QByteArray deviceUpdateRequest;
    QByteArray deviceDeleteRequest;
    QByteArray geocodingRequest;
    QByteArray forecastRequest;
    QByteArray profileBody = QByteArrayLiteral(
        R"([{"id":"123e4567-e89b-12d3-a456-426614174000","first_name":"Amina","city":null}])");
    QByteArray updatedProfileBody = QByteArrayLiteral(
        R"([{"id":"123e4567-e89b-12d3-a456-426614174000","first_name":"Amina K","city":"Nairobi"}])");
    QByteArray roomsBody = QByteArrayLiteral("[]");
    QByteArray insertedRoomBody = QByteArrayLiteral(
        R"([{"id":"123e4567-e89b-12d3-a456-426614174011","name":"Office","position":3}])");
    QByteArray updatedRoomBody = QByteArrayLiteral(
        R"([{"id":"123e4567-e89b-12d3-a456-426614174010","name":"Lounge","position":0}])");
    QByteArray insertedDeviceBody = QByteArrayLiteral(
        R"([{"id":"123e4567-e89b-12d3-a456-426614174020","room_id":"123e4567-e89b-12d3-a456-426614174010","name":"Desk light","kind":"light","is_on":false,"celsius":null,"reading_at":null,"position":0}])");
    QByteArray geocodingBody = QByteArrayLiteral(
        R"({"results":[{"name":"Nairobi","latitude":-1.29,"longitude":36.82}]})");
    QByteArray forecastBody = QByteArrayLiteral(
        R"({"current":{"temperature_2m":20.9,"weather_code":2,"is_day":1}})");
    bool failDeviceToggle = false;
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
            } else if (requestLine.startsWith(QByteArrayLiteral("GET /rest/v1/rooms?select="))) {
                state.roomsRequest = *request;
                payload = state.roomsBody;
            } else if (requestLine.startsWith(QByteArrayLiteral("POST /rest/v1/rooms?select="))) {
                state.roomInsertRequest = *request;
                status = 201;
                reason = QByteArrayLiteral("Created");
                payload = state.insertedRoomBody;
            } else if (requestLine.startsWith(QByteArrayLiteral("PATCH /rest/v1/rooms?id=eq."))) {
                state.roomUpdateRequest = *request;
                payload = state.updatedRoomBody;
            } else if (requestLine.startsWith(QByteArrayLiteral("DELETE /rest/v1/rooms?id=eq."))) {
                state.roomDeleteRequest = *request;
                payload = QByteArrayLiteral(R"([{"id":"123e4567-e89b-12d3-a456-426614174010"}])");
            } else if (requestLine.startsWith(QByteArrayLiteral("POST /rest/v1/devices?select="))) {
                state.deviceInsertRequest = *request;
                status = 201;
                reason = QByteArrayLiteral("Created");
                payload = state.insertedDeviceBody;
            } else if (requestLine.startsWith(QByteArrayLiteral("PATCH /rest/v1/devices?id=eq."))) {
                const QJsonObject patch = QJsonDocument::fromJson(
                    request->mid(headerEnd + 4)).object();
                QJsonObject row = QJsonDocument::fromJson(state.insertedDeviceBody).array().first().toObject();
                if (patch.contains(QStringLiteral("celsius"))) {
                    state.deviceReadingRequest = *request;
                    row.insert(QStringLiteral("id"), QStringLiteral("123e4567-e89b-12d3-a456-426614174030"));
                    row.insert(QStringLiteral("room_id"), QStringLiteral("123e4567-e89b-12d3-a456-426614174010"));
                    row.insert(QStringLiteral("name"), QStringLiteral("Old thermometer"));
                    row.insert(QStringLiteral("position"), 0);
                    row.insert(QStringLiteral("kind"), QStringLiteral("thermometer"));
                    row.insert(QStringLiteral("is_on"), QJsonValue(QJsonValue::Null));
                    row.insert(QStringLiteral("celsius"), patch.value(QStringLiteral("celsius")));
                    row.insert(QStringLiteral("reading_at"), patch.value(QStringLiteral("reading_at")));
                    payload = QJsonDocument(QJsonArray{row}).toJson(QJsonDocument::Compact);
                } else if (patch.contains(QStringLiteral("name"))) {
                    state.deviceUpdateRequest = *request;
                    row.insert(QStringLiteral("name"), patch.value(QStringLiteral("name")));
                    payload = QJsonDocument(QJsonArray{row}).toJson(QJsonDocument::Compact);
                } else if (state.failDeviceToggle) {
                    state.deviceToggleRequest = *request;
                    status = 500;
                    reason = QByteArrayLiteral("Internal Server Error");
                    payload = QByteArrayLiteral(R"({"message":"switch write rejected"})");
                } else {
                    state.deviceToggleRequest = *request;
                    row.insert(QStringLiteral("is_on"), patch.value(QStringLiteral("is_on")));
                    payload = QJsonDocument(QJsonArray{row}).toJson(QJsonDocument::Compact);
                }
            } else if (requestLine.startsWith(QByteArrayLiteral("DELETE /rest/v1/devices?id=eq."))) {
                state.deviceDeleteRequest = *request;
                payload = QByteArrayLiteral(R"([{"id":"123e4567-e89b-12d3-a456-426614174020"}])");
            } else if (requestLine.startsWith(QByteArrayLiteral("GET /geocode?"))) {
                state.geocodingRequest = *request;
                payload = state.geocodingBody;
            } else if (requestLine.startsWith(QByteArrayLiteral("GET /forecast?"))) {
                state.forecastRequest = *request;
                payload = state.forecastBody;
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
    void loadsAndCreatesRooms();
    void createsDeviceInExistingRoom();
    void togglesDeviceAndRollsBackOnFailure();
    void renamesAndDeletesDevice();
    void renamesAndDeletesRoom();
    void updatesOnlyStaleThermometer();
    void loadsWeatherForProfileCity();
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
    QTRY_COMPARE_WITH_TIMEOUT(loadRefreshToken(tokenPath), QStringLiteral("rotated-refresh-token"), 5000);
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

    QTRY_COMPARE_WITH_TIMEOUT(loadRefreshToken(tokenPath), QStringLiteral("rotated-refresh-token"), 5000);
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

    const QString baseUrl = QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort());
    ApiClient::WeatherEndpoints weatherEndpoints{
        QUrl(baseUrl + QStringLiteral("/geocode")),
        QUrl(baseUrl + QStringLiteral("/forecast"))};
    SessionController controller(baseUrl, QStringLiteral("public-anon-key"),
        directory.filePath(QStringLiteral("refresh-token.bin")), weatherEndpoints);
    QVERIFY(controller.signIn(QStringLiteral("person@example.com"), QStringLiteral("correct-horse")));
    QTRY_COMPARE_WITH_TIMEOUT(controller.firstName(), QStringLiteral("Amina"), 5000);
    QVERIFY(controller.greeting().endsWith(QStringLiteral("Amina")));
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

void SessionControllerTest::loadsAndCreatesRooms()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    AuthServerState state{sessionResponse()};
    state.roomsBody = QByteArrayLiteral(
        R"([{"id":"123e4567-e89b-12d3-a456-426614174010","name":"Living room","position":2,"devices":[]}])");
    startAuthServer(server, state);

    SessionController controller(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()),
        QStringLiteral("public-anon-key"), directory.filePath(QStringLiteral("refresh-token.bin")));
    QVERIFY(controller.signIn(QStringLiteral("person@example.com"), QStringLiteral("correct-horse")));
    QTRY_COMPARE_WITH_TIMEOUT(controller.rooms().size(), 1, 5000);

    const QVariantMap existingRoom = controller.rooms().first().toMap();
    QCOMPARE(existingRoom.value(QStringLiteral("id")).toString(),
        QStringLiteral("123e4567-e89b-12d3-a456-426614174010"));
    QCOMPARE(existingRoom.value(QStringLiteral("name")).toString(), QStringLiteral("Living room"));
    QCOMPARE(existingRoom.value(QStringLiteral("position")).toInt(), 2);
    QVERIFY(state.roomsRequest.left(state.roomsRequest.indexOf(QByteArrayLiteral("\r\n\r\n")))
        .toLower().contains(QByteArrayLiteral("\r\nauthorization: bearer access-token")));

    QVERIFY(!controller.createRoom(QString()));
    QVERIFY(state.roomInsertRequest.isEmpty());
    QVERIFY(controller.createRoom(QStringLiteral(" Office ")));
    QTRY_COMPARE_WITH_TIMEOUT(controller.rooms().size(), 2, 5000);

    const QVariantMap createdRoom = controller.rooms().last().toMap();
    QCOMPARE(createdRoom.value(QStringLiteral("name")).toString(), QStringLiteral("Office"));
    QCOMPARE(createdRoom.value(QStringLiteral("position")).toInt(), 3);
    QVERIFY(state.roomInsertRequest.left(state.roomInsertRequest.indexOf(QByteArrayLiteral("\r\n\r\n")))
        .toLower().contains(QByteArrayLiteral("\r\nauthorization: bearer access-token")));
    const QJsonObject body = requestBody(state.roomInsertRequest);
    QCOMPARE(body.value(QStringLiteral("name")).toString(), QStringLiteral("Office"));
    QCOMPARE(body.value(QStringLiteral("position")).toInt(), 3);
}

void SessionControllerTest::createsDeviceInExistingRoom()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    AuthServerState state{sessionResponse()};
    state.roomsBody = QByteArrayLiteral(
        R"([{"id":"123e4567-e89b-12d3-a456-426614174010","name":"Living room","position":0,"devices":[]}])");
    startAuthServer(server, state);

    SessionController controller(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()),
        QStringLiteral("public-anon-key"), directory.filePath(QStringLiteral("refresh-token.bin")));
    QVERIFY(controller.signIn(QStringLiteral("person@example.com"), QStringLiteral("correct-horse")));
    QTRY_COMPARE_WITH_TIMEOUT(controller.rooms().size(), 1, 5000);

    const QString roomId = QStringLiteral("123e4567-e89b-12d3-a456-426614174010");
    QVERIFY(!controller.createDevice(roomId, QString(), QStringLiteral("light")));
    QVERIFY(!controller.createDevice(roomId, QStringLiteral("Desk light"), QStringLiteral("camera")));
    QVERIFY(!controller.createDevice(QStringLiteral("123e4567-e89b-12d3-a456-426614174099"),
        QStringLiteral("Desk light"), QStringLiteral("light")));
    QVERIFY(state.deviceInsertRequest.isEmpty());

    QVERIFY(controller.createDevice(roomId, QStringLiteral(" Desk light "), QStringLiteral("light")));
    QTRY_COMPARE_WITH_TIMEOUT(controller.rooms().first().toMap().value(QStringLiteral("devices")).toList().size(),
        1, 5000);

    const QVariantMap room = controller.rooms().first().toMap();
    QCOMPARE(room.value(QStringLiteral("deviceCount")).toInt(), 1);
    const QVariantMap device = room.value(QStringLiteral("devices")).toList().first().toMap();
    QCOMPARE(device.value(QStringLiteral("deviceId")).toString(),
        QStringLiteral("123e4567-e89b-12d3-a456-426614174020"));
    QCOMPARE(device.value(QStringLiteral("name")).toString(), QStringLiteral("Desk light"));
    QCOMPARE(device.value(QStringLiteral("kind")).toString(), QStringLiteral("light"));
    QVERIFY(!device.value(QStringLiteral("isOn")).toBool());

    const qsizetype headerEnd = state.deviceInsertRequest.indexOf(QByteArrayLiteral("\r\n\r\n"));
    QVERIFY(state.deviceInsertRequest.left(headerEnd).toLower()
        .contains(QByteArrayLiteral("\r\nauthorization: bearer access-token")));
    const QJsonObject body = requestBody(state.deviceInsertRequest);
    QCOMPARE(body.value(QStringLiteral("room_id")).toString(), roomId);
    QCOMPARE(body.value(QStringLiteral("name")).toString(), QStringLiteral("Desk light"));
    QCOMPARE(body.value(QStringLiteral("kind")).toString(), QStringLiteral("light"));
    QCOMPARE(body.value(QStringLiteral("position")).toInt(), 0);
    QCOMPARE(body.value(QStringLiteral("is_on")).toBool(), false);
}

void SessionControllerTest::togglesDeviceAndRollsBackOnFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    AuthServerState state{sessionResponse()};
    state.roomsBody = QByteArrayLiteral(
        R"([{"id":"123e4567-e89b-12d3-a456-426614174010","name":"Living room","position":0,"devices":[{"id":"123e4567-e89b-12d3-a456-426614174020","room_id":"123e4567-e89b-12d3-a456-426614174010","name":"Desk light","kind":"light","is_on":false,"celsius":null,"reading_at":null,"position":0}]}])");
    startAuthServer(server, state);

    SessionController controller(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()),
        QStringLiteral("public-anon-key"), directory.filePath(QStringLiteral("refresh-token.bin")));
    QVERIFY(controller.signIn(QStringLiteral("person@example.com"), QStringLiteral("correct-horse")));
    QTRY_COMPARE_WITH_TIMEOUT(controller.rooms().size(), 1, 5000);
    const QString deviceId = QStringLiteral("123e4567-e89b-12d3-a456-426614174020");

    QVERIFY(controller.setDeviceOn(deviceId, true));
    QTRY_VERIFY_WITH_TIMEOUT(controller.rooms().first().toMap().value(QStringLiteral("devices"))
        .toList().first().toMap().value(QStringLiteral("isOn")).toBool(), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(controller.setDeviceOn(deviceId, true), 5000);
    const qsizetype successHeaderEnd = state.deviceToggleRequest.indexOf(QByteArrayLiteral("\r\n\r\n"));
    QVERIFY(state.deviceToggleRequest.left(successHeaderEnd).toLower()
        .contains(QByteArrayLiteral("\r\nauthorization: bearer access-token")));
    QCOMPARE(requestBody(state.deviceToggleRequest).value(QStringLiteral("is_on")).toBool(), true);

    state.failDeviceToggle = true;
    QVERIFY(controller.setDeviceOn(deviceId, false));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.statusMessage().isEmpty(), 5000);
    QCOMPARE(controller.rooms().first().toMap().value(QStringLiteral("devices"))
        .toList().first().toMap().value(QStringLiteral("isOn")).toBool(), true);
    QCOMPARE(requestBody(state.deviceToggleRequest).value(QStringLiteral("is_on")).toBool(), false);
}

void SessionControllerTest::renamesAndDeletesDevice()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    AuthServerState state{sessionResponse()};
    state.roomsBody = QByteArrayLiteral(
        R"([{"id":"123e4567-e89b-12d3-a456-426614174010","name":"Living room","position":0,"devices":[{"id":"123e4567-e89b-12d3-a456-426614174020","room_id":"123e4567-e89b-12d3-a456-426614174010","name":"Desk light","kind":"light","is_on":false,"celsius":null,"reading_at":null,"position":0}]}])");
    startAuthServer(server, state);

    SessionController controller(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()),
        QStringLiteral("public-anon-key"), directory.filePath(QStringLiteral("refresh-token.bin")));
    QVERIFY(controller.signIn(QStringLiteral("person@example.com"), QStringLiteral("correct-horse")));
    QTRY_COMPARE_WITH_TIMEOUT(controller.rooms().size(), 1, 5000);
    const QString deviceId = QStringLiteral("123e4567-e89b-12d3-a456-426614174020");

    QVERIFY(!controller.renameDevice(deviceId, QString()));
    QVERIFY(state.deviceUpdateRequest.isEmpty());
    QVERIFY(controller.renameDevice(deviceId, QStringLiteral(" Desk lamp ")));
    QTRY_COMPARE_WITH_TIMEOUT(controller.rooms().first().toMap().value(QStringLiteral("devices"))
        .toList().first().toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Desk lamp"), 5000);
    const qsizetype updateHeaderEnd = state.deviceUpdateRequest.indexOf(QByteArrayLiteral("\r\n\r\n"));
    QVERIFY(state.deviceUpdateRequest.left(updateHeaderEnd).toLower()
        .contains(QByteArrayLiteral("\r\nauthorization: bearer access-token")));
    QCOMPARE(requestBody(state.deviceUpdateRequest).value(QStringLiteral("name")).toString(),
        QStringLiteral("Desk lamp"));

    QVERIFY(!controller.deleteDevice(QStringLiteral("not-a-uuid")));
    QVERIFY(state.deviceDeleteRequest.isEmpty());
    QVERIFY(controller.deleteDevice(deviceId));
    QTRY_COMPARE_WITH_TIMEOUT(controller.rooms().first().toMap().value(QStringLiteral("devices")).toList().size(),
        0, 5000);
    QVERIFY(state.deviceDeleteRequest.startsWith(QByteArrayLiteral("DELETE /rest/v1/devices?id=eq.")));
    QVERIFY(state.deviceDeleteRequest.toLower().contains(
        QByteArrayLiteral("authorization: bearer access-token")));
}

void SessionControllerTest::renamesAndDeletesRoom()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    AuthServerState state{sessionResponse()};
    state.roomsBody = QByteArrayLiteral(
        R"([{"id":"123e4567-e89b-12d3-a456-426614174010","name":"Living room","position":0,"devices":[{"id":"123e4567-e89b-12d3-a456-426614174020","room_id":"123e4567-e89b-12d3-a456-426614174010","name":"Desk light","kind":"light","is_on":false,"celsius":null,"reading_at":null,"position":0}]}])");
    startAuthServer(server, state);

    SessionController controller(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()),
        QStringLiteral("public-anon-key"), directory.filePath(QStringLiteral("refresh-token.bin")));
    QVERIFY(controller.signIn(QStringLiteral("person@example.com"), QStringLiteral("correct-horse")));
    QTRY_COMPARE_WITH_TIMEOUT(controller.rooms().size(), 1, 5000);
    const QString roomId = QStringLiteral("123e4567-e89b-12d3-a456-426614174010");

    QVERIFY(!controller.renameRoom(roomId, QString()));
    QVERIFY(!controller.renameRoom(QStringLiteral("123e4567-e89b-12d3-a456-426614174099"),
        QStringLiteral("Lounge")));
    QVERIFY(state.roomUpdateRequest.isEmpty());
    QVERIFY(controller.renameRoom(roomId, QStringLiteral(" Lounge ")));
    QTRY_COMPARE_WITH_TIMEOUT(controller.rooms().first().toMap().value(QStringLiteral("name")).toString(),
        QStringLiteral("Lounge"), 5000);
    QCOMPARE(controller.rooms().first().toMap().value(QStringLiteral("devices")).toList().size(), 1);
    const qsizetype updateHeaderEnd = state.roomUpdateRequest.indexOf(QByteArrayLiteral("\r\n\r\n"));
    QVERIFY(state.roomUpdateRequest.left(updateHeaderEnd).toLower()
        .contains(QByteArrayLiteral("\r\nauthorization: bearer access-token")));
    QCOMPARE(requestBody(state.roomUpdateRequest).value(QStringLiteral("name")).toString(),
        QStringLiteral("Lounge"));

    QVERIFY(!controller.deleteRoom(QStringLiteral("not-a-uuid")));
    QVERIFY(state.roomDeleteRequest.isEmpty());
    QVERIFY(controller.deleteRoom(roomId));
    QTRY_COMPARE_WITH_TIMEOUT(controller.rooms().size(), 0, 5000);
    QVERIFY(state.roomDeleteRequest.startsWith(QByteArrayLiteral("DELETE /rest/v1/rooms?id=eq.")));
    QVERIFY(state.roomDeleteRequest.toLower().contains(
        QByteArrayLiteral("authorization: bearer access-token")));
}

void SessionControllerTest::updatesOnlyStaleThermometer()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    AuthServerState state{sessionResponse()};
    const QString freshReadingAt = QDateTime::currentDateTimeUtc().addSecs(-30).toString(Qt::ISODateWithMs);
    const QJsonArray devices{
        QJsonObject{{QStringLiteral("id"), QStringLiteral("123e4567-e89b-12d3-a456-426614174030")},
            {QStringLiteral("room_id"), QStringLiteral("123e4567-e89b-12d3-a456-426614174010")},
            {QStringLiteral("name"), QStringLiteral("Old thermometer")},
            {QStringLiteral("kind"), QStringLiteral("thermometer")},
            {QStringLiteral("is_on"), QJsonValue(QJsonValue::Null)},
            {QStringLiteral("celsius"), 23.4},
            {QStringLiteral("reading_at"), QStringLiteral("2000-01-01T00:00:00+00:00")},
            {QStringLiteral("position"), 0}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("123e4567-e89b-12d3-a456-426614174031")},
            {QStringLiteral("room_id"), QStringLiteral("123e4567-e89b-12d3-a456-426614174010")},
            {QStringLiteral("name"), QStringLiteral("Fresh thermometer")},
            {QStringLiteral("kind"), QStringLiteral("thermometer")},
            {QStringLiteral("is_on"), QJsonValue(QJsonValue::Null)},
            {QStringLiteral("celsius"), 22.4},
            {QStringLiteral("reading_at"), freshReadingAt},
            {QStringLiteral("position"), 1}}};
    state.roomsBody = QJsonDocument(QJsonArray{QJsonObject{
        {QStringLiteral("id"), QStringLiteral("123e4567-e89b-12d3-a456-426614174010")},
        {QStringLiteral("name"), QStringLiteral("Living room")},
        {QStringLiteral("position"), 0},
        {QStringLiteral("devices"), devices}}}).toJson(QJsonDocument::Compact);
    startAuthServer(server, state);

    SessionController controller(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()),
        QStringLiteral("public-anon-key"), directory.filePath(QStringLiteral("refresh-token.bin")));
    QVERIFY(controller.signIn(QStringLiteral("person@example.com"), QStringLiteral("correct-horse")));
    QTRY_VERIFY_WITH_TIMEOUT(!state.deviceReadingRequest.isEmpty(), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(controller.rooms().first().toMap().value(QStringLiteral("devices"))
        .toList().first().toMap().value(QStringLiteral("celsius")).toDouble() != 23.4, 5000);

    QCOMPARE(state.deviceReadingRequest.startsWith(QByteArrayLiteral("PATCH /rest/v1/devices?id=eq.123e4567-e89b-12d3-a456-426614174030")), true);
    QVERIFY(state.deviceReadingRequest.left(state.deviceReadingRequest.indexOf(QByteArrayLiteral("\r\n\r\n")))
        .toLower().contains(QByteArrayLiteral("\r\nauthorization: bearer access-token")));
    const QJsonObject reading = requestBody(state.deviceReadingRequest);
    QVERIFY(reading.value(QStringLiteral("celsius")).toDouble() >= 18.0);
    QVERIFY(reading.value(QStringLiteral("celsius")).toDouble() <= 28.0);
    QVERIFY(QDateTime::fromString(reading.value(QStringLiteral("reading_at")).toString(), Qt::ISODateWithMs).isValid());
    QCOMPARE(controller.rooms().first().toMap().value(QStringLiteral("devices")).toList()
        .at(1).toMap().value(QStringLiteral("celsius")).toDouble(), 22.4);
}

void SessionControllerTest::loadsWeatherForProfileCity()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    AuthServerState state{sessionResponse()};
    state.profileBody = QByteArrayLiteral(
        R"([{"id":"123e4567-e89b-12d3-a456-426614174000","first_name":"Amina","city":"Nairobi"}])");
    startAuthServer(server, state);

    const QString baseUrl = QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort());
    ApiClient::WeatherEndpoints weatherEndpoints{
        QUrl(baseUrl + QStringLiteral("/geocode")),
        QUrl(baseUrl + QStringLiteral("/forecast"))};
    SessionController controller(baseUrl, QStringLiteral("public-anon-key"),
        directory.filePath(QStringLiteral("refresh-token.bin")), weatherEndpoints);
    QVERIFY(controller.signIn(QStringLiteral("person@example.com"), QStringLiteral("correct-horse")));
    QTRY_COMPARE_WITH_TIMEOUT(controller.weatherLine(), QStringLiteral("Partly cloudy · 20.9 °C"), 5000);

    QVERIFY(state.geocodingRequest.contains(QByteArrayLiteral("name=Nairobi")));
    QVERIFY(state.forecastRequest.contains(QByteArrayLiteral("latitude=-1.290000")));
    QVERIFY(state.forecastRequest.contains(QByteArrayLiteral("longitude=36.820000")));
    QVERIFY(!state.geocodingRequest.toLower().contains(QByteArrayLiteral("authorization:")));
    QVERIFY(!state.forecastRequest.toLower().contains(QByteArrayLiteral("authorization:")));
    QCOMPARE(controller.weatherIcon(), QStringLiteral("qrc:/qt/qml/SmartHome/assets/icons/sun-cloud.svg"));
}

QTEST_MAIN(SessionControllerTest)
#include "tst_session_controller.moc"
