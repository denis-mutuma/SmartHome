#include "session_controller.h"

#include "token_store.h"

#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>

#include <memory>
#include <utility>

namespace {

QString uiServiceUrl;
QString uiTokenPath;

class UiSessionController : public SessionController
{
    Q_OBJECT
public:
    explicit UiSessionController(QObject* parent = nullptr)
        : SessionController(uiServiceUrl, QStringLiteral("public-anon-key"), uiTokenPath, parent)
    {
    }
};

struct DeferredResponse
{
    QTcpSocket* socket;
    int status;
    QByteArray reason;
    QByteArray payload;
};

struct AuthServerState
{
    explicit AuthServerState(QByteArray body)
        : sessionBody(std::move(body))
    {
    }

    QByteArray sessionBody;
    bool deferAuthResponses = false;
    QList<DeferredResponse> pendingAuthResponses;
    QByteArray refreshBody;
    QByteArray loginRequest;
    QByteArray signupRequest;
    QByteArray refreshRequest;
    QByteArray logoutRequest;
    QByteArray profileRequest;
    QByteArray profileUpdateRequest;
    int profileUpdateRequests = 0;
    QByteArray roomsRequest;
    QByteArray roomInsertRequest;
    QByteArray roomUpdateRequest;
    int roomUpdateRequests = 0;
    QByteArray roomDeleteRequest;
    QByteArray roomDeleteBody = QByteArrayLiteral(
        R"([{"id":"123e4567-e89b-12d3-a456-426614174010"}])");
    bool deferRoomDeleteResponses = false;
    QList<DeferredResponse> pendingRoomDeleteResponses;
    QByteArray deviceInsertRequest;
    QByteArray deviceToggleRequest;
    int deviceToggleRequests = 0;
    bool deferDeviceToggleResponses = false;
    QList<DeferredResponse> pendingDeviceToggleResponses;
    QByteArray deviceReadingRequest;
    QByteArray deviceUpdateRequest;
    QByteArray deviceDeleteRequest;
    QByteArray deviceDeleteBody = QByteArrayLiteral(
        R"([{"id":"123e4567-e89b-12d3-a456-426614174020","room_id":"123e4567-e89b-12d3-a456-426614174010"}])");
    bool deferDeviceDeleteResponses = false;
    QList<DeferredResponse> pendingDeviceDeleteResponses;
    QByteArray geocodingRequest;
    QByteArray forecastRequest;
    int profileRequests = 0;
    int roomsRequests = 0;
    int deviceUpdateRequests = 0;
    int geocodingRequests = 0;
    int forecastRequests = 0;
    bool deferProfileResponses = false;
    bool deferRoomsResponses = false;
    bool deferDeviceUpdateResponses = false;
    bool deferGeocodingResponses = false;
    bool deferForecastResponses = false;
    QList<DeferredResponse> pendingProfileResponses;
    QList<DeferredResponse> pendingRoomsResponses;
    QList<DeferredResponse> pendingDeviceUpdateResponses;
    QList<DeferredResponse> pendingGeocodingResponses;
    QList<DeferredResponse> pendingForecastResponses;
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
    bool failRoomInsert = false;
    bool failDeviceInsert = false;
    bool deferRoomInsertResponses = false;
    bool deferDeviceInsertResponses = false;
    int refreshRequests = 0;
    int refreshStatus = 200;
    int profileUnauthorizedResponses = 0;
    QList<DeferredResponse> pendingRoomInsertResponses;
    QList<DeferredResponse> pendingDeviceInsertResponses;
};

void writeResponse(const DeferredResponse& response)
{
    response.socket->write(QByteArrayLiteral("HTTP/1.1 ") + QByteArray::number(response.status) + ' '
        + response.reason + QByteArrayLiteral("\r\nContent-Type: application/json\r\nContent-Length: ")
        + QByteArray::number(response.payload.size()) + QByteArrayLiteral("\r\nConnection: close\r\n\r\n")
        + response.payload);
    response.socket->disconnectFromHost();
}

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
            QList<DeferredResponse>* deferredResponses = nullptr;
            if (path == QByteArrayLiteral("/auth/v1/token?grant_type=password")) {
                state.loginRequest = *request;
                if (state.deferAuthResponses) {
                    deferredResponses = &state.pendingAuthResponses;
                }
            } else if (path == QByteArrayLiteral("/auth/v1/signup")) {
                state.signupRequest = *request;
                if (state.deferAuthResponses) {
                    deferredResponses = &state.pendingAuthResponses;
                }
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
                ++state.profileRequests;
                state.profileRequest = *request;
                if (state.profileUnauthorizedResponses > 0) {
                    --state.profileUnauthorizedResponses;
                    status = 401;
                    reason = QByteArrayLiteral("Unauthorized");
                    payload = QByteArrayLiteral(R"({"message":"access token expired"})");
                } else {
                    payload = state.profileBody;
                }
                if (state.deferProfileResponses) {
                    deferredResponses = &state.pendingProfileResponses;
                }
            } else if (requestLine.startsWith(QByteArrayLiteral("PATCH /rest/v1/profiles?id=eq."))) {
                ++state.profileUpdateRequests;
                state.profileUpdateRequest = *request;
                payload = state.updatedProfileBody;
            } else if (requestLine.startsWith(QByteArrayLiteral("GET /rest/v1/rooms?select="))) {
                ++state.roomsRequests;
                state.roomsRequest = *request;
                payload = state.roomsBody;
                if (state.deferRoomsResponses) {
                    deferredResponses = &state.pendingRoomsResponses;
                }
            } else if (requestLine.startsWith(QByteArrayLiteral("POST /rest/v1/rooms?select="))) {
                state.roomInsertRequest = *request;
                if (state.failRoomInsert) {
                    status = 500;
                    reason = QByteArrayLiteral("Internal Server Error");
                    payload = QByteArrayLiteral(R"({"message":"room insert rejected"})");
                } else {
                    status = 201;
                    reason = QByteArrayLiteral("Created");
                    payload = state.insertedRoomBody;
                }
                if (state.deferRoomInsertResponses) {
                    deferredResponses = &state.pendingRoomInsertResponses;
                }
            } else if (requestLine.startsWith(QByteArrayLiteral("PATCH /rest/v1/rooms?id=eq."))) {
                ++state.roomUpdateRequests;
                state.roomUpdateRequest = *request;
                payload = state.updatedRoomBody;
            } else if (requestLine.startsWith(QByteArrayLiteral("DELETE /rest/v1/rooms?id=eq."))) {
                state.roomDeleteRequest = *request;
                payload = state.roomDeleteBody;
                if (state.deferRoomDeleteResponses) {
                    deferredResponses = &state.pendingRoomDeleteResponses;
                }
            } else if (requestLine.startsWith(QByteArrayLiteral("POST /rest/v1/devices?select="))) {
                state.deviceInsertRequest = *request;
                if (state.failDeviceInsert) {
                    status = 500;
                    reason = QByteArrayLiteral("Internal Server Error");
                    payload = QByteArrayLiteral(R"({"message":"device insert rejected"})");
                } else {
                    status = 201;
                    reason = QByteArrayLiteral("Created");
                    payload = state.insertedDeviceBody;
                }
                if (state.deferDeviceInsertResponses) {
                    deferredResponses = &state.pendingDeviceInsertResponses;
                }
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
                    ++state.deviceUpdateRequests;
                    state.deviceUpdateRequest = *request;
                    row.insert(QStringLiteral("name"), patch.value(QStringLiteral("name")));
                    payload = QJsonDocument(QJsonArray{row}).toJson(QJsonDocument::Compact);
                    if (state.deferDeviceUpdateResponses) {
                        deferredResponses = &state.pendingDeviceUpdateResponses;
                    }
                } else if (state.failDeviceToggle) {
                    ++state.deviceToggleRequests;
                    state.deviceToggleRequest = *request;
                    status = 500;
                    reason = QByteArrayLiteral("Internal Server Error");
                    payload = QByteArrayLiteral(R"({"message":"switch write rejected"})");
                } else {
                    ++state.deviceToggleRequests;
                    state.deviceToggleRequest = *request;
                    const qsizetype idStart = requestLine.indexOf(QByteArrayLiteral("id=eq."))
                        + qsizetype(sizeof("id=eq.") - 1);
                    const qsizetype idEnd = requestLine.indexOf('&', idStart);
                    row.insert(QStringLiteral("id"), QString::fromLatin1(requestLine.mid(idStart,
                        idEnd - idStart)));
                    row.insert(QStringLiteral("is_on"), patch.value(QStringLiteral("is_on")));
                    payload = QJsonDocument(QJsonArray{row}).toJson(QJsonDocument::Compact);
                    if (state.deferDeviceToggleResponses) {
                        deferredResponses = &state.pendingDeviceToggleResponses;
                    }
                }
            } else if (requestLine.startsWith(QByteArrayLiteral("DELETE /rest/v1/devices?id=eq."))) {
                state.deviceDeleteRequest = *request;
                payload = state.deviceDeleteBody;
                if (state.deferDeviceDeleteResponses) {
                    deferredResponses = &state.pendingDeviceDeleteResponses;
                }
            } else if (requestLine.startsWith(QByteArrayLiteral("GET /geocode?"))) {
                ++state.geocodingRequests;
                state.geocodingRequest = *request;
                payload = state.geocodingBody;
                if (state.deferGeocodingResponses) {
                    deferredResponses = &state.pendingGeocodingResponses;
                }
            } else if (requestLine.startsWith(QByteArrayLiteral("GET /forecast?"))) {
                ++state.forecastRequests;
                state.forecastRequest = *request;
                payload = state.forecastBody;
                if (state.deferForecastResponses) {
                    deferredResponses = &state.pendingForecastResponses;
                }
            } else {
                return;
            }
            socket->setProperty("responded", true);
            const DeferredResponse response{socket, status, reason, payload};
            if (deferredResponses != nullptr) {
                deferredResponses->append(response);
            } else {
                writeResponse(response);
            }
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

bool setPollingState(SessionController& controller, Qt::ApplicationState state)
{
    return QMetaObject::invokeMethod(&controller, "updatePolling", Qt::DirectConnection,
        Q_ARG(Qt::ApplicationState, state));
}

QByteArray profileResponse(const QString& firstName)
{
    return QJsonDocument(QJsonArray{QJsonObject{
        {QStringLiteral("id"), QStringLiteral("123e4567-e89b-12d3-a456-426614174000")},
        {QStringLiteral("first_name"), firstName},
        {QStringLiteral("city"), QJsonValue(QJsonValue::Null)}}})
        .toJson(QJsonDocument::Compact);
}

QByteArray roomResponse(const QString& deviceName)
{
    const QString roomId = QStringLiteral("123e4567-e89b-12d3-a456-426614174010");
    const QString deviceId = QStringLiteral("123e4567-e89b-12d3-a456-426614174020");
    return QJsonDocument(QJsonArray{QJsonObject{
        {QStringLiteral("id"), roomId},
        {QStringLiteral("name"), QStringLiteral("Office")},
        {QStringLiteral("position"), 0},
        {QStringLiteral("devices"), QJsonArray{QJsonObject{
            {QStringLiteral("id"), deviceId},
            {QStringLiteral("room_id"), roomId},
            {QStringLiteral("name"), deviceName},
            {QStringLiteral("kind"), QStringLiteral("light")},
            {QStringLiteral("is_on"), false},
            {QStringLiteral("celsius"), QJsonValue(QJsonValue::Null)},
            {QStringLiteral("reading_at"), QJsonValue(QJsonValue::Null)},
            {QStringLiteral("position"), 0}}}}}})
        .toJson(QJsonDocument::Compact);
}

QByteArray geocodingResponse(const QString& name, double latitude, double longitude)
{
    return QJsonDocument(QJsonObject{{QStringLiteral("results"), QJsonArray{QJsonObject{
        {QStringLiteral("name"), name},
        {QStringLiteral("latitude"), latitude},
        {QStringLiteral("longitude"), longitude}}}}})
        .toJson(QJsonDocument::Compact);
}

QByteArray forecastResponse(double temperature)
{
    return QJsonDocument(QJsonObject{{QStringLiteral("current"), QJsonObject{
        {QStringLiteral("temperature_2m"), temperature},
        {QStringLiteral("weather_code"), 0},
        {QStringLiteral("is_day"), 1}}}})
        .toJson(QJsonDocument::Compact);
}

} // namespace

class SessionControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() {
        qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
        qmlRegisterType<UiSessionController>("SmartHome", 1, 0, "SessionController");
    }
    void uiRenameWaitsForValidatedSuccess();
    void uiClearsCredentialsAndResetsOnSessionEnd();
    void serializesAuthenticationAndRecoversAfterCancellation();
    void signInPersistsRefreshTokenAndSignOutClearsIt();
    void registerSendsNormalizedAccountDetails();
    void rejectsInvalidCredentials();
    void restoresSessionWithRotatedRefreshToken();
    void retriesStoredSessionAfterTransientRefreshFailure();
    void clearsRejectedStoredSession();
    void transientRefreshFailurePreservesSession();
    void rejectedRefreshDiscardsQueuedRequests();
    void retriesUnauthorizedRequestOnceAfterRefresh();
    void refreshesNearExpirySessionOnce();
    void loadsAndSavesProfileSettings();
    void loadsAndCreatesRooms();
    void createsDeviceInExistingRoom();
    void createSignalsOnlyOnSuccess();
    void signOutFailsPendingCreates();
    void togglesDeviceAndRollsBackOnFailure();
    void togglesIndependentDevicesConcurrently();
    void renamesAndDeletesDevice();
    void renamesAndDeletesRoom();
    void updatesOnlyStaleThermometer();
    void updatesThermometerWithNullReadingTime();
    void loadsWeatherForProfileCity();
    void pollingFollowsSignInAndApplicationState();
    void reloadFetchesProfileAndRooms();
    void serializesSameDeviceMutations();
    void ignoresOlderWeatherForecastResponse();
    void ignoresWeatherResponseAfterCityCleared();
};

void SessionControllerTest::uiClearsCredentialsAndResetsOnSessionEnd()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    AuthServerState state{sessionResponse()};
    startAuthServer(server, state);
    uiServiceUrl = QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort());
    uiTokenPath = directory.filePath(QStringLiteral("session.bin"));
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(QFINDTESTDATA("../Main.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> window(component.create());
    QVERIFY(window);
    QQmlContext* context = qmlContext(window.get());
    auto* controller = qobject_cast<SessionController*>(context->objectForName(QStringLiteral("session")));
    QObject* password = context->objectForName(QStringLiteral("passwordInput"));
    QVERIFY(controller);
    QVERIFY(password);
    for (bool rejectRefresh : {false, true}) {
        QVERIFY(password->setProperty("text", QStringLiteral("correct-horse")));
        QVERIFY(!controller->signIn(QStringLiteral("invalid"), QStringLiteral("correct-horse")));
        QCOMPARE(password->property("text").toString(), QStringLiteral("correct-horse"));
        QVERIFY(controller->signIn(QStringLiteral("person@example.com"), QStringLiteral("correct-horse")));
        QTRY_VERIFY_WITH_TIMEOUT(controller->signedIn(), 5000);
        QVERIFY(password->property("text").toString().isEmpty());
        QVERIFY(window->setProperty("selectedRoomId", QStringLiteral("old-room")));
        QVERIFY(window->setProperty("settingsOpen", true));
        QVERIFY(window->setProperty("roomCreatePending", true));
        QVERIFY(window->setProperty("deviceCreatePending", true));
        QVERIFY(window->setProperty("actionEntityId", QStringLiteral("old-device")));
        QObject* dialog = context->objectForName(QStringLiteral("renameDialog"));
        QVERIFY(QMetaObject::invokeMethod(dialog, "open"));
        QTRY_VERIFY_WITH_TIMEOUT(dialog->property("visible").toBool(), 5000);
        if (rejectRefresh) {
            state.profileUnauthorizedResponses = 1;
            state.refreshStatus = 401;
            controller->reload();
        } else {
            controller->signOut();
        }
        QTRY_VERIFY_WITH_TIMEOUT(!controller->signedIn(), 5000);
        QVERIFY(window->property("selectedRoomId").toString().isEmpty());
        QVERIFY(!window->property("settingsOpen").toBool());
        QVERIFY(!window->property("roomCreatePending").toBool());
        QVERIFY(!window->property("deviceCreatePending").toBool());
        QVERIFY(window->property("actionEntityId").toString().isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(!dialog->property("visible").toBool(), 5000);
    }
}

void SessionControllerTest::uiRenameWaitsForValidatedSuccess()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    AuthServerState state{sessionResponse()};
    state.roomsBody = roomResponse(QStringLiteral("Desk light"));
    state.deferDeviceUpdateResponses = true;
    startAuthServer(server, state);
    uiServiceUrl = QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort());
    uiTokenPath = directory.filePath(QStringLiteral("session.bin"));
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(QFINDTESTDATA("../Main.qml")));
    std::unique_ptr<QObject> window(component.create());
    QVERIFY2(window, qPrintable(component.errorString()));
    QQmlContext* context = qmlContext(window.get());
    auto* controller = qobject_cast<SessionController*>(context->objectForName(QStringLiteral("session")));
    QVERIFY(controller->signIn(QStringLiteral("person@example.com"), QStringLiteral("correct-horse")));
    QTRY_COMPARE_WITH_TIMEOUT(controller->rooms().size(), 1, 5000);
    window->setProperty("actionEntityType", QStringLiteral("device"));
    window->setProperty("actionEntityId", QStringLiteral("123e4567-e89b-12d3-a456-426614174020"));
    QObject* dialog = context->objectForName(QStringLiteral("renameDialog"));
    QObject* input = context->objectForName(QStringLiteral("renameInput"));
    QObject* save = context->objectForName(QStringLiteral("renameSave"));
    QVERIFY(dialog && input && save);
    input->setProperty("text", QStringLiteral("New light"));
    QVERIFY(QMetaObject::invokeMethod(dialog, "open"));
    QVERIFY(QMetaObject::invokeMethod(save, "clicked"));
    QTRY_COMPARE_WITH_TIMEOUT(state.pendingDeviceUpdateResponses.size(), 1, 5000);
    QVERIFY(dialog->property("visible").toBool());
    QVERIFY(!save->property("enabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(save, "clicked"));
    QCOMPARE(state.deviceUpdateRequests, 1);
    controller->renameFinished(QStringLiteral("room:unrelated"), true);
    QVERIFY(dialog->property("visible").toBool());
    DeferredResponse failed = state.pendingDeviceUpdateResponses.takeFirst();
    failed.status = 500;
    failed.payload = QByteArrayLiteral(R"({"message":"rename rejected"})");
    writeResponse(failed);
    QTRY_COMPARE_WITH_TIMEOUT(dialog->property("errorMessage").toString(), QStringLiteral("rename rejected"), 5000);
    QVERIFY(dialog->property("visible").toBool());
    QCOMPARE(input->property("text").toString(), QStringLiteral("New light"));
    QVERIFY(QMetaObject::invokeMethod(save, "clicked"));
    QTRY_COMPARE_WITH_TIMEOUT(state.pendingDeviceUpdateResponses.size(), 1, 5000);
    writeResponse(state.pendingDeviceUpdateResponses.takeFirst());
    QTRY_VERIFY_WITH_TIMEOUT(!dialog->property("visible").toBool(), 5000);
    QVERIFY(input->property("text").toString().isEmpty());
    QVERIFY(QMetaObject::invokeMethod(dialog, "open"));
    QVERIFY(QMetaObject::invokeMethod(save, "clicked"));
    QVERIFY(dialog->property("pendingKey").toString().isEmpty());
    QVERIFY(!dialog->property("errorMessage").toString().isEmpty());
    input->setProperty("text", QStringLiteral("Pending rename"));
    QVERIFY(QMetaObject::invokeMethod(save, "clicked"));
    QTRY_COMPARE_WITH_TIMEOUT(state.pendingDeviceUpdateResponses.size(), 1, 5000);
    controller->signOut();
    QTRY_VERIFY_WITH_TIMEOUT(!dialog->property("visible").toBool(), 5000);
    QVERIFY(dialog->property("pendingKey").toString().isEmpty());
}

void SessionControllerTest::serializesAuthenticationAndRecoversAfterCancellation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    AuthServerState state{sessionResponse()};
    state.deferAuthResponses = true;
    startAuthServer(server, state);
    SessionController controller(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()),
        QStringLiteral("public-anon-key"), directory.filePath(QStringLiteral("refresh-token.bin")));
    const QString email = QStringLiteral("person@example.com");
    const QString password = QStringLiteral("correct-horse");
    QVERIFY(controller.signIn(email, password));
    QTRY_COMPARE_WITH_TIMEOUT(state.pendingAuthResponses.size(), 1, 5000);
    QVERIFY(!controller.signIn(email, password));
    QVERIFY(!controller.registerAccount(QStringLiteral("Amina"), email, password));
    DeferredResponse rejected = state.pendingAuthResponses.takeFirst();
    rejected.status = 400;
    rejected.reason = QByteArrayLiteral("Bad Request");
    rejected.payload = QByteArrayLiteral(R"({"message":"login rejected"})");
    writeResponse(rejected);
    QTRY_COMPARE_WITH_TIMEOUT(controller.statusMessage(), QStringLiteral("login rejected"), 5000);
    bool reentered = false;
    const auto statusConnection = connect(&controller, &SessionController::statusChanged, &controller, [&]() {
        if (controller.statusMessage().isEmpty()) {
            reentered = true;
            QVERIFY(!controller.signIn(email, password));
            QVERIFY(!controller.registerAccount(QStringLiteral("Amina"), email, password));
        }
    });
    QVERIFY(controller.registerAccount(QStringLiteral("Amina"), email, password));
    QVERIFY(reentered);
    disconnect(statusConnection);
    QTRY_COMPARE_WITH_TIMEOUT(state.pendingAuthResponses.size(), 1, 5000);
    QVERIFY(!controller.signIn(email, password));
    QVERIFY(!controller.registerAccount(QStringLiteral("Amina"), email, password));
    controller.signOut();
    QVERIFY(controller.signIn(email, password));
    QTRY_COMPARE_WITH_TIMEOUT(state.pendingAuthResponses.size(), 2, 5000);
    writeResponse(state.pendingAuthResponses.takeLast());
    QTRY_VERIFY_WITH_TIMEOUT(controller.signedIn(), 5000);
    QVERIFY(!controller.signIn(email, password));
    QVERIFY(!controller.registerAccount(QStringLiteral("Amina"), email, password));
}

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

void SessionControllerTest::retriesStoredSessionAfterTransientRefreshFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString tokenPath = directory.filePath(QStringLiteral("refresh-token.bin"));
    QVERIFY(saveRefreshToken(QStringLiteral("old-refresh-token"), tokenPath));

    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    AuthServerState state{sessionResponse()};
    state.refreshStatus = 500;
    state.refreshBody = sessionResponse(QStringLiteral("new-access-token"),
        QStringLiteral("rotated-refresh-token"));
    startAuthServer(server, state);

    SessionController controller(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()),
        QStringLiteral("public-anon-key"), tokenPath);
    QSignalSpy statusChanged(&controller, &SessionController::statusChanged);
    QVERIFY(statusChanged.isValid());
    QTRY_COMPARE_WITH_TIMEOUT(state.refreshRequests, 1, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(statusChanged.count() > 0, 5000);
    QVERIFY(!controller.signedIn());
    QCOMPARE(loadRefreshToken(tokenPath), QStringLiteral("old-refresh-token"));

    state.refreshStatus = 200;
    QVERIFY(QMetaObject::invokeMethod(&controller, "refreshIfNeeded", Qt::DirectConnection));
    QTRY_VERIFY_WITH_TIMEOUT(controller.signedIn(), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(state.refreshRequests, 2, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(loadRefreshToken(tokenPath), QStringLiteral("rotated-refresh-token"), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(controller.firstName(), QStringLiteral("Amina"), 5000);
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

void SessionControllerTest::transientRefreshFailurePreservesSession()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString tokenPath = directory.filePath(QStringLiteral("refresh-token.bin"));
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    AuthServerState state{sessionResponse(QStringLiteral("long-lived-access-token"),
        QStringLiteral("old-refresh-token"), 3600)};
    state.refreshStatus = 500;
    state.refreshBody = sessionResponse(QStringLiteral("new-access-token"),
        QStringLiteral("rotated-refresh-token"));
    startAuthServer(server, state);

    SessionController controller(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()),
        QStringLiteral("public-anon-key"), tokenPath);
    QVERIFY(controller.signIn(QStringLiteral("person@example.com"), QStringLiteral("correct-horse")));
    QTRY_COMPARE_WITH_TIMEOUT(controller.firstName(), QStringLiteral("Amina"), 5000);
    QSignalSpy statusChanged(&controller, &SessionController::statusChanged);
    QVERIFY(statusChanged.isValid());

    state.profileUnauthorizedResponses = 1;
    controller.reload();
    QTRY_COMPARE_WITH_TIMEOUT(state.refreshRequests, 1, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(statusChanged.count() > 0, 5000);
    QVERIFY(controller.signedIn());
    QCOMPARE(loadRefreshToken(tokenPath), QStringLiteral("old-refresh-token"));

    state.refreshStatus = 200;
    QVERIFY(QMetaObject::invokeMethod(&controller, "refreshIfNeeded", Qt::DirectConnection));
    QTRY_COMPARE_WITH_TIMEOUT(state.refreshRequests, 2, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(loadRefreshToken(tokenPath), QStringLiteral("rotated-refresh-token"), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(state.profileRequests, 3, 5000);
    QVERIFY(controller.signedIn());
}

void SessionControllerTest::rejectedRefreshDiscardsQueuedRequests()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString tokenPath = directory.filePath(QStringLiteral("refresh-token.bin"));
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    AuthServerState state{sessionResponse(QStringLiteral("old-access-token"),
        QStringLiteral("old-refresh-token"))};
    startAuthServer(server, state);

    SessionController controller(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()),
        QStringLiteral("public-anon-key"), tokenPath);
    QVERIFY(controller.signIn(QStringLiteral("person@example.com"), QStringLiteral("correct-horse")));
    QTRY_COMPARE_WITH_TIMEOUT(controller.firstName(), QStringLiteral("Amina"), 5000);
    state.profileUnauthorizedResponses = 1;
    state.refreshStatus = 401;
    controller.reload();
    QTRY_COMPARE_WITH_TIMEOUT(state.profileRequests, 2, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(state.refreshRequests, 1, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(!controller.signedIn(), 5000);
    QVERIFY(loadRefreshToken(tokenPath).isEmpty());

    state.sessionBody = sessionResponse(QStringLiteral("new-access-token"),
        QStringLiteral("new-refresh-token"), 30);
    state.refreshStatus = 200;
    state.refreshBody = sessionResponse(QStringLiteral("rotated-access-token"),
        QStringLiteral("rotated-refresh-token"));
    QVERIFY(controller.signIn(QStringLiteral("person@example.com"), QStringLiteral("correct-horse")));
    QTRY_COMPARE_WITH_TIMEOUT(state.profileRequests, 3, 5000);
    QVERIFY(controller.signedIn());
    QVERIFY(QMetaObject::invokeMethod(&controller, "refreshIfNeeded", Qt::DirectConnection));
    QTRY_COMPARE_WITH_TIMEOUT(state.refreshRequests, 2, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(loadRefreshToken(tokenPath), QStringLiteral("rotated-refresh-token"), 5000);
    QTest::qWait(100);
    QCOMPARE(state.profileRequests, 3);
    QVERIFY(controller.signedIn());
}

void SessionControllerTest::retriesUnauthorizedRequestOnceAfterRefresh()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    AuthServerState state{sessionResponse()};
    state.refreshBody = sessionResponse(QStringLiteral("new-access-token"),
        QStringLiteral("rotated-refresh-token"));
    startAuthServer(server, state);

    SessionController controller(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()),
        QStringLiteral("public-anon-key"), directory.filePath(QStringLiteral("refresh-token.bin")));
    QVERIFY(controller.signIn(QStringLiteral("person@example.com"), QStringLiteral("correct-horse")));
    QTRY_COMPARE_WITH_TIMEOUT(controller.firstName(), QStringLiteral("Amina"), 5000);
    state.profileUnauthorizedResponses = 2;
    controller.reload();

    QTRY_COMPARE_WITH_TIMEOUT(state.refreshRequests, 1, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(state.profileRequests, 3, 5000);
    QCOMPARE(state.profileUnauthorizedResponses, 0);
    QVERIFY(state.profileRequest.toLower().contains(QByteArrayLiteral("authorization: bearer new-access-token")));
    QVERIFY(controller.signedIn());
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
    QVERIFY(!controller.saveSettings(QStringLiteral("Amina L"), QStringLiteral("Mombasa")));
    QTRY_COMPARE_WITH_TIMEOUT(state.profileUpdateRequests, 1, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(controller.firstName(), QStringLiteral("Amina K"), 5000);
    QCOMPARE(controller.city(), QStringLiteral("Nairobi"));

    const qsizetype headerEnd = state.profileUpdateRequest.indexOf(QByteArrayLiteral("\r\n\r\n"));
    QVERIFY(state.profileUpdateRequest.left(headerEnd).toLower()
        .contains(QByteArrayLiteral("\r\nauthorization: bearer access-token")));
    const QJsonObject body = requestBody(state.profileUpdateRequest);
    QCOMPARE(body.value(QStringLiteral("first_name")).toString(), QStringLiteral("Amina K"));
    QCOMPARE(body.value(QStringLiteral("city")).toString(), QStringLiteral("Nairobi"));
    QVERIFY(controller.saveSettings(QStringLiteral("Amina L"), QStringLiteral("Mombasa")));
    QTRY_COMPARE_WITH_TIMEOUT(state.profileUpdateRequests, 2, 5000);
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

void SessionControllerTest::createSignalsOnlyOnSuccess()
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
    QSignalSpy roomCreated(&controller, &SessionController::roomCreated);
    QSignalSpy roomCreateFailed(&controller, &SessionController::roomCreateFailed);
    QSignalSpy deviceCreated(&controller, &SessionController::deviceCreated);
    QSignalSpy deviceCreateFailed(&controller, &SessionController::deviceCreateFailed);
    QVERIFY(roomCreated.isValid());
    QVERIFY(roomCreateFailed.isValid());
    QVERIFY(deviceCreated.isValid());
    QVERIFY(deviceCreateFailed.isValid());
    QVERIFY(controller.signIn(QStringLiteral("person@example.com"), QStringLiteral("correct-horse")));
    QTRY_COMPARE_WITH_TIMEOUT(controller.rooms().size(), 1, 5000);

    state.failRoomInsert = true;
    QVERIFY(controller.createRoom(QStringLiteral("Office")));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.statusMessage().isEmpty(), 5000);
    QCOMPARE(roomCreated.count(), 0);
    QTRY_COMPARE_WITH_TIMEOUT(roomCreateFailed.count(), 1, 5000);
    QCOMPARE(roomCreateFailed.first().first().toString(), QStringLiteral("Office"));
    state.failRoomInsert = false;
    QVERIFY(controller.createRoom(QStringLiteral("Office")));
    QTRY_COMPARE_WITH_TIMEOUT(roomCreated.count(), 1, 5000);
    QCOMPARE(roomCreated.first().first().toString(), QStringLiteral("Office"));

    const QString roomId = QStringLiteral("123e4567-e89b-12d3-a456-426614174010");
    state.failDeviceInsert = true;
    QVERIFY(controller.createDevice(roomId, QStringLiteral("Desk light"), QStringLiteral("light")));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.statusMessage().isEmpty(), 5000);
    QCOMPARE(deviceCreated.count(), 0);
    QTRY_COMPARE_WITH_TIMEOUT(deviceCreateFailed.count(), 1, 5000);
    QCOMPARE(deviceCreateFailed.first().at(0).toString(), roomId);
    QCOMPARE(deviceCreateFailed.first().at(1).toString(), QStringLiteral("Desk light"));
    state.failDeviceInsert = false;
    QVERIFY(controller.createDevice(roomId, QStringLiteral("Desk light"), QStringLiteral("light")));
    QTRY_COMPARE_WITH_TIMEOUT(deviceCreated.count(), 1, 5000);
    QCOMPARE(deviceCreated.first().at(0).toString(), roomId);
    QCOMPARE(deviceCreated.first().at(1).toString(), QStringLiteral("Desk light"));
}

void SessionControllerTest::signOutFailsPendingCreates()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    AuthServerState state{sessionResponse()};
    state.roomsBody = QByteArrayLiteral(
        R"([{"id":"123e4567-e89b-12d3-a456-426614174010","name":"Living room","position":0,"devices":[]}])");
    state.deferRoomInsertResponses = true;
    state.deferDeviceInsertResponses = true;
    startAuthServer(server, state);

    SessionController controller(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()),
        QStringLiteral("public-anon-key"), directory.filePath(QStringLiteral("refresh-token.bin")));
    QSignalSpy roomCreateFailed(&controller, &SessionController::roomCreateFailed);
    QSignalSpy deviceCreateFailed(&controller, &SessionController::deviceCreateFailed);
    QVERIFY(controller.signIn(QStringLiteral("person@example.com"), QStringLiteral("correct-horse")));
    QTRY_COMPARE_WITH_TIMEOUT(controller.rooms().size(), 1, 5000);
    const QString roomId = QStringLiteral("123e4567-e89b-12d3-a456-426614174010");
    QVERIFY(controller.createRoom(QStringLiteral("Office")));
    QVERIFY(controller.createDevice(roomId, QStringLiteral("Desk light"), QStringLiteral("light")));
    QTRY_COMPARE_WITH_TIMEOUT(state.pendingRoomInsertResponses.size(), 1, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(state.pendingDeviceInsertResponses.size(), 1, 5000);

    controller.signOut();
    QCOMPARE(roomCreateFailed.count(), 1);
    QCOMPARE(roomCreateFailed.first().first().toString(), QStringLiteral("Office"));
    QCOMPARE(deviceCreateFailed.count(), 1);
    QCOMPARE(deviceCreateFailed.first().at(0).toString(), roomId);
    QCOMPARE(deviceCreateFailed.first().at(1).toString(), QStringLiteral("Desk light"));
}

void SessionControllerTest::togglesDeviceAndRollsBackOnFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    AuthServerState state{sessionResponse()};
    state.roomsBody = QByteArrayLiteral(
        R"([{"id":"123e4567-e89b-12d3-a456-426614174010","name":"Living room","position":0,"devices":[{"id":"123e4567-e89b-12d3-a456-426614174020","room_id":"123e4567-e89b-12d3-a456-426614174010","name":"Desk light","kind":"light","is_on":false,"celsius":null,"reading_at":null,"position":0},{"id":"123e4567-e89b-12d3-a456-426614174021","room_id":"123e4567-e89b-12d3-a456-426614174010","name":"Other light","kind":"light","is_on":false,"celsius":null,"reading_at":null,"position":1}]}])");
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

void SessionControllerTest::togglesIndependentDevicesConcurrently()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    AuthServerState state{sessionResponse()};
    state.roomsBody = QByteArrayLiteral(
        R"([{"id":"123e4567-e89b-12d3-a456-426614174010","name":"Living room","position":0,"devices":[{"id":"123e4567-e89b-12d3-a456-426614174020","room_id":"123e4567-e89b-12d3-a456-426614174010","name":"Desk light","kind":"light","is_on":false,"celsius":null,"reading_at":null,"position":0},{"id":"123e4567-e89b-12d3-a456-426614174021","room_id":"123e4567-e89b-12d3-a456-426614174010","name":"Floor lamp","kind":"light","is_on":false,"celsius":null,"reading_at":null,"position":1}]}])");
    state.deferDeviceToggleResponses = true;
    startAuthServer(server, state);

    SessionController controller(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()),
        QStringLiteral("public-anon-key"), directory.filePath(QStringLiteral("refresh-token.bin")));
    QVERIFY(controller.signIn(QStringLiteral("person@example.com"), QStringLiteral("correct-horse")));
    QTRY_COMPARE_WITH_TIMEOUT(controller.rooms().size(), 1, 5000);

    const QString firstId = QStringLiteral("123e4567-e89b-12d3-a456-426614174020");
    const QString secondId = QStringLiteral("123e4567-e89b-12d3-a456-426614174021");
    QVERIFY(controller.setDeviceOn(firstId, true));
    QTRY_COMPARE_WITH_TIMEOUT(state.pendingDeviceToggleResponses.size(), 1, 5000);
    QVERIFY(!controller.setDeviceOn(firstId, false));
    QVERIFY(controller.setDeviceOn(secondId, true));
    QTRY_COMPARE_WITH_TIMEOUT(state.pendingDeviceToggleResponses.size(), 2, 5000);
    QCOMPARE(state.deviceToggleRequests, 2);

    writeResponse(state.pendingDeviceToggleResponses.takeAt(1));
    QTRY_VERIFY_WITH_TIMEOUT(controller.rooms().first().toMap().value(QStringLiteral("devices"))
        .toList().at(1).toMap().value(QStringLiteral("isOn")).toBool(), 5000);
    writeResponse(state.pendingDeviceToggleResponses.takeFirst());
    QTRY_VERIFY_WITH_TIMEOUT(controller.rooms().first().toMap().value(QStringLiteral("devices"))
        .toList().at(0).toMap().value(QStringLiteral("isOn")).toBool(), 5000);
}

void SessionControllerTest::renamesAndDeletesDevice()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    AuthServerState state{sessionResponse()};
    state.roomsBody = QByteArrayLiteral(
        R"([{"id":"123e4567-e89b-12d3-a456-426614174010","name":"Living room","position":0,"devices":[{"id":"123e4567-e89b-12d3-a456-426614174020","room_id":"123e4567-e89b-12d3-a456-426614174010","name":"Desk light","kind":"light","is_on":false,"celsius":null,"reading_at":null,"position":0},{"id":"123e4567-e89b-12d3-a456-426614174021","room_id":"123e4567-e89b-12d3-a456-426614174010","name":"Floor lamp","kind":"light","is_on":false,"celsius":null,"reading_at":null,"position":1}]}])");
    startAuthServer(server, state);

    SessionController controller(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()),
        QStringLiteral("public-anon-key"), directory.filePath(QStringLiteral("refresh-token.bin")));
    QSignalSpy renamed(&controller, &SessionController::renameFinished);
    QSignalSpy deleted(&controller, &SessionController::deleteFinished);
    QVERIFY(renamed.isValid());
    QVERIFY(deleted.isValid());
    QVERIFY(controller.signIn(QStringLiteral("person@example.com"), QStringLiteral("correct-horse")));
    QTRY_COMPARE_WITH_TIMEOUT(controller.rooms().size(), 1, 5000);
    const QString deviceId = QStringLiteral("123e4567-e89b-12d3-a456-426614174020");

    QVERIFY(!controller.renameDevice(deviceId, QString()));
    QVERIFY(state.deviceUpdateRequest.isEmpty());
    state.deferDeviceUpdateResponses = true;
    QVERIFY(controller.renameDevice(deviceId, QStringLiteral(" Desk lamp ")));
    QTRY_COMPARE_WITH_TIMEOUT(state.deviceUpdateRequests, 1, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(state.pendingDeviceUpdateResponses.size(), 1, 5000);
    QVERIFY(!controller.renameDevice(deviceId, QStringLiteral("Reading lamp")));
    QCOMPARE(state.deviceUpdateRequests, 1);
    const QByteArray firstUpdateRequest = state.deviceUpdateRequest;
    writeResponse(state.pendingDeviceUpdateResponses.takeFirst());
    QTRY_COMPARE_WITH_TIMEOUT(controller.rooms().first().toMap().value(QStringLiteral("devices"))
        .toList().first().toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Desk lamp"), 5000);
    state.deferDeviceUpdateResponses = false;
    QVERIFY(controller.renameDevice(deviceId, QStringLiteral("Reading lamp")));
    QTRY_COMPARE_WITH_TIMEOUT(controller.rooms().first().toMap().value(QStringLiteral("devices"))
        .toList().first().toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Reading lamp"), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(renamed.count(), 2, 5000);
    QCOMPARE(renamed.last().at(0).toString(), QStringLiteral("device:") + deviceId);
    QVERIFY(renamed.last().at(1).toBool());
    const qsizetype updateHeaderEnd = firstUpdateRequest.indexOf(QByteArrayLiteral("\r\n\r\n"));
    QVERIFY(firstUpdateRequest.left(updateHeaderEnd).toLower()
        .contains(QByteArrayLiteral("\r\nauthorization: bearer access-token")));
    QCOMPARE(requestBody(firstUpdateRequest).value(QStringLiteral("name")).toString(),
        QStringLiteral("Desk lamp"));

    state.deferDeviceUpdateResponses = true;
    for (bool httpFailure : {true, false}) {
        const QVariantList previousRooms = controller.rooms();
        const int previousResults = renamed.count();
        QVERIFY(controller.renameDevice(deviceId, QStringLiteral("Rejected rename")));
        QTRY_COMPARE_WITH_TIMEOUT(state.pendingDeviceUpdateResponses.size(), 1, 5000);
        DeferredResponse response = state.pendingDeviceUpdateResponses.takeFirst();
        if (httpFailure) {
            response.status = 500;
            response.reason = QByteArrayLiteral("Internal Server Error");
            response.payload = QByteArrayLiteral(R"({"message":"rename rejected"})");
        } else {
            QJsonObject row = QJsonDocument::fromJson(response.payload).array().first().toObject();
            row.insert(QStringLiteral("room_id"), QStringLiteral("123e4567-e89b-12d3-a456-426614174011"));
            response.payload = QJsonDocument(QJsonArray{row}).toJson(QJsonDocument::Compact);
        }
        writeResponse(response);
        QTRY_COMPARE_WITH_TIMEOUT(renamed.count(), previousResults + 1, 5000);
        QCOMPARE(renamed.last().at(0).toString(), QStringLiteral("device:") + deviceId);
        QVERIFY(!renamed.last().at(1).toBool());
        QCOMPARE(controller.rooms(), previousRooms);
    }

    QVERIFY(!controller.deleteDevice(QStringLiteral("not-a-uuid")));
    QVERIFY(state.deviceDeleteRequest.isEmpty());
    state.deferDeviceDeleteResponses = true;
    const QString siblingDeviceId = QStringLiteral("123e4567-e89b-12d3-a456-426614174021");
    for (const QByteArray& response : {
             QByteArrayLiteral(R"([{"id":"123e4567-e89b-12d3-a456-426614174020","room_id":"123e4567-e89b-12d3-a456-426614174010"},{}])"),
             QByteArrayLiteral(R"([{"id":"123e4567-e89b-12d3-a456-426614174021","room_id":"123e4567-e89b-12d3-a456-426614174010"}])"),
             QByteArrayLiteral(R"([{"id":"123e4567-e89b-12d3-a456-426614174020","room_id":"123e4567-e89b-12d3-a456-426614174011"}])")}) {
        const QVariantList previousRooms = controller.rooms();
        const int previousDeletes = deleted.count();
        state.deviceDeleteBody = response;
        QVERIFY(controller.deleteDevice(deviceId));
        QTRY_COMPARE_WITH_TIMEOUT(state.pendingDeviceDeleteResponses.size(), 1, 5000);
        writeResponse(state.pendingDeviceDeleteResponses.takeFirst());
        QTRY_COMPARE_WITH_TIMEOUT(deleted.count(), previousDeletes + 1, 5000);
        QCOMPARE(deleted.last().at(0).toString(), QStringLiteral("device:") + deviceId);
        QVERIFY(!deleted.last().at(1).toBool());
        QTRY_VERIFY_WITH_TIMEOUT(controller.statusMessage().contains(QStringLiteral("service could not complete")),
            5000);
        QCOMPARE(controller.rooms(), previousRooms);
        QCOMPARE(controller.rooms().first().toMap().value(QStringLiteral("devices")).toList().size(), 2);
        QCOMPARE(controller.rooms().first().toMap().value(QStringLiteral("devices")).toList()
            .at(1).toMap().value(QStringLiteral("deviceId")).toString(), siblingDeviceId);
    }
    const QVariantList previousRooms = controller.rooms();
    const int previousDeletes = deleted.count();
    QVERIFY(controller.deleteDevice(deviceId));
    QTRY_COMPARE_WITH_TIMEOUT(state.pendingDeviceDeleteResponses.size(), 1, 5000);
    DeferredResponse failedDelete = state.pendingDeviceDeleteResponses.takeFirst();
    failedDelete.status = 500;
    failedDelete.reason = QByteArrayLiteral("Internal Server Error");
    failedDelete.payload = QByteArrayLiteral(R"({"message":"delete rejected"})");
    writeResponse(failedDelete);
    QTRY_COMPARE_WITH_TIMEOUT(deleted.count(), previousDeletes + 1, 5000);
    QCOMPARE(deleted.last().at(0).toString(), QStringLiteral("device:") + deviceId);
    QVERIFY(!deleted.last().at(1).toBool());
    QCOMPARE(controller.rooms(), previousRooms);

    state.deferDeviceDeleteResponses = false;
    state.deviceDeleteBody = QByteArrayLiteral(
        R"([{"id":"123e4567-e89b-12d3-a456-426614174020","room_id":"123e4567-e89b-12d3-a456-426614174010"}])");
    QVERIFY(controller.deleteDevice(deviceId));
    QTRY_COMPARE_WITH_TIMEOUT(controller.rooms().first().toMap().value(QStringLiteral("devices")).toList().size(),
        1, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(deleted.count(), previousDeletes + 2, 5000);
    QCOMPARE(deleted.last().at(0).toString(), QStringLiteral("device:") + deviceId);
    QVERIFY(deleted.last().at(1).toBool());
    QVERIFY(state.deviceDeleteRequest.startsWith(QByteArrayLiteral("DELETE /rest/v1/devices?id=eq.")));
    QVERIFY(state.deviceDeleteRequest.toLower().contains(
        QByteArrayLiteral("authorization: bearer access-token")));
    QVERIFY(state.deviceDeleteRequest.contains(QByteArrayLiteral("select=id,room_id")));
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
    QJsonArray rooms = QJsonDocument::fromJson(state.roomsBody).array();
    rooms.append(QJsonObject{{QStringLiteral("id"), QStringLiteral("123e4567-e89b-12d3-a456-426614174011")},
        {QStringLiteral("name"), QStringLiteral("Other room")}, {QStringLiteral("position"), 1}});
    state.roomsBody = QJsonDocument(rooms).toJson(QJsonDocument::Compact);
    startAuthServer(server, state);

    SessionController controller(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()),
        QStringLiteral("public-anon-key"), directory.filePath(QStringLiteral("refresh-token.bin")));
    QSignalSpy renamed(&controller, &SessionController::renameFinished);
    QSignalSpy deleted(&controller, &SessionController::deleteFinished);
    QVERIFY(renamed.isValid());
    QVERIFY(deleted.isValid());
    QVERIFY(controller.signIn(QStringLiteral("person@example.com"), QStringLiteral("correct-horse")));
    QTRY_COMPARE_WITH_TIMEOUT(controller.rooms().size(), 2, 5000);
    const QString roomId = QStringLiteral("123e4567-e89b-12d3-a456-426614174010");

    QVERIFY(!controller.renameRoom(roomId, QString()));
    QVERIFY(!controller.renameRoom(QStringLiteral("123e4567-e89b-12d3-a456-426614174099"),
        QStringLiteral("Lounge")));
    QVERIFY(state.roomUpdateRequest.isEmpty());
    QVERIFY(controller.renameRoom(roomId, QStringLiteral(" Lounge ")));
    QVERIFY(!controller.renameRoom(roomId, QStringLiteral("Bedroom")));
    QVERIFY(!controller.deleteRoom(roomId));
    QVERIFY(state.roomDeleteRequest.isEmpty());
    QTRY_COMPARE_WITH_TIMEOUT(state.roomUpdateRequests, 1, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(controller.rooms().first().toMap().value(QStringLiteral("name")).toString(),
        QStringLiteral("Lounge"), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(renamed.count(), 1, 5000);
    QCOMPARE(renamed.first().at(0).toString(), QStringLiteral("room:") + roomId);
    QVERIFY(renamed.first().at(1).toBool());
    QCOMPARE(controller.rooms().first().toMap().value(QStringLiteral("devices")).toList().size(), 1);
    const qsizetype updateHeaderEnd = state.roomUpdateRequest.indexOf(QByteArrayLiteral("\r\n\r\n"));
    QVERIFY(state.roomUpdateRequest.left(updateHeaderEnd).toLower()
        .contains(QByteArrayLiteral("\r\nauthorization: bearer access-token")));
    QCOMPARE(requestBody(state.roomUpdateRequest).value(QStringLiteral("name")).toString(),
        QStringLiteral("Lounge"));

    state.updatedRoomBody = QByteArrayLiteral(
        R"([{"id":"123e4567-e89b-12d3-a456-426614174010","name":"Bedroom","position":0}])");
    QVERIFY(controller.renameRoom(roomId, QStringLiteral("Bedroom")));
    QTRY_COMPARE_WITH_TIMEOUT(state.roomUpdateRequests, 2, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(controller.rooms().first().toMap().value(QStringLiteral("name")).toString(),
        QStringLiteral("Bedroom"), 5000);
    QCOMPARE(requestBody(state.roomUpdateRequest).value(QStringLiteral("name")).toString(),
        QStringLiteral("Bedroom"));

    for (const QByteArray& response : {QByteArrayLiteral("[]"), QByteArrayLiteral(
             R"([{"id":"123e4567-e89b-12d3-a456-426614174010","name":"Wrong shape","position":0},{}])"), QByteArrayLiteral(
             R"([{"id":"123e4567-e89b-12d3-a456-426614174011","name":"Wrong room","position":0}])")}) {
        state.updatedRoomBody = response;
        const int previousResults = renamed.count();
        const QVariantList previousRooms = controller.rooms();
        QVERIFY(controller.renameRoom(roomId, QStringLiteral("Rejected rename")));
        QTRY_COMPARE_WITH_TIMEOUT(renamed.count(), previousResults + 1, 5000);
        QCOMPARE(renamed.last().at(0).toString(), QStringLiteral("room:") + roomId);
        QVERIFY(!renamed.last().at(1).toBool());
        QCOMPARE(controller.rooms(), previousRooms);
    }

    QVERIFY(!controller.deleteRoom(QStringLiteral("not-a-uuid")));
    QVERIFY(state.roomDeleteRequest.isEmpty());
    state.deferRoomDeleteResponses = true;
    for (const QByteArray& response : {
             QByteArrayLiteral(R"([{"id":"123e4567-e89b-12d3-a456-426614174010"},{}])"),
             QByteArrayLiteral(R"([{"id":"123e4567-e89b-12d3-a456-426614174011"}])")}) {
        const QVariantList previousRooms = controller.rooms();
        const int previousDeletes = deleted.count();
        state.roomDeleteBody = response;
        QVERIFY(controller.deleteRoom(roomId));
        QTRY_COMPARE_WITH_TIMEOUT(state.pendingRoomDeleteResponses.size(), 1, 5000);
        writeResponse(state.pendingRoomDeleteResponses.takeFirst());
        QTRY_COMPARE_WITH_TIMEOUT(deleted.count(), previousDeletes + 1, 5000);
        QCOMPARE(deleted.last().at(0).toString(), QStringLiteral("room:") + roomId);
        QVERIFY(!deleted.last().at(1).toBool());
        QTRY_VERIFY_WITH_TIMEOUT(controller.statusMessage().contains(QStringLiteral("service could not complete")),
            5000);
        QCOMPARE(controller.rooms(), previousRooms);
    }
    const QVariantList previousRooms = controller.rooms();
    int previousDeletes = deleted.count();
    QVERIFY(controller.deleteRoom(roomId));
    QTRY_COMPARE_WITH_TIMEOUT(state.pendingRoomDeleteResponses.size(), 1, 5000);
    DeferredResponse failedDelete = state.pendingRoomDeleteResponses.takeFirst();
    failedDelete.status = 500;
    failedDelete.reason = QByteArrayLiteral("Internal Server Error");
    failedDelete.payload = QByteArrayLiteral(R"({"message":"delete rejected"})");
    writeResponse(failedDelete);
    QTRY_COMPARE_WITH_TIMEOUT(deleted.count(), previousDeletes + 1, 5000);
    QCOMPARE(deleted.last().at(0).toString(), QStringLiteral("room:") + roomId);
    QVERIFY(!deleted.last().at(1).toBool());
    QCOMPARE(controller.rooms(), previousRooms);

    state.deferRoomDeleteResponses = false;
    state.roomDeleteBody = QByteArrayLiteral(
        R"([{"id":"123e4567-e89b-12d3-a456-426614174010"}])");
    previousDeletes = deleted.count();
    QVERIFY(controller.deleteRoom(roomId));
    QTRY_COMPARE_WITH_TIMEOUT(controller.rooms().size(), 1, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(deleted.count(), previousDeletes + 1, 5000);
    QCOMPARE(deleted.last().at(0).toString(), QStringLiteral("room:") + roomId);
    QVERIFY(deleted.last().at(1).toBool());
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

void SessionControllerTest::updatesThermometerWithNullReadingTime()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    AuthServerState state{sessionResponse()};
    state.roomsBody = QByteArrayLiteral(
        R"([{"id":"123e4567-e89b-12d3-a456-426614174010","name":"Living room","position":0,"devices":[{"id":"123e4567-e89b-12d3-a456-426614174030","room_id":"123e4567-e89b-12d3-a456-426614174010","name":"Probe","kind":"thermometer","is_on":null,"celsius":22.0,"reading_at":null,"position":0}]}])");
    startAuthServer(server, state);

    SessionController controller(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()),
        QStringLiteral("public-anon-key"), directory.filePath(QStringLiteral("refresh-token.bin")));
    QVERIFY(controller.signIn(QStringLiteral("person@example.com"), QStringLiteral("correct-horse")));
    QTRY_VERIFY_WITH_TIMEOUT(!state.deviceReadingRequest.isEmpty(), 5000);
    const QJsonObject reading = requestBody(state.deviceReadingRequest);
    QVERIFY(QDateTime::fromString(reading.value(QStringLiteral("reading_at")).toString(), Qt::ISODateWithMs)
        .isValid());
    const double updatedCelsius = reading.value(QStringLiteral("celsius")).toDouble();
    QVERIFY(updatedCelsius >= 18.0 && updatedCelsius <= 28.0);
    QTRY_COMPARE_WITH_TIMEOUT(controller.rooms().first().toMap().value(QStringLiteral("devices")).toList()
                                  .first().toMap().value(QStringLiteral("celsius")).toDouble(),
        updatedCelsius, 5000);
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
    state.deferGeocodingResponses = true;
    startAuthServer(server, state);

    const QString baseUrl = QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort());
    ApiClient::WeatherEndpoints weatherEndpoints{
        QUrl(baseUrl + QStringLiteral("/geocode")),
        QUrl(baseUrl + QStringLiteral("/forecast"))};
    SessionController controller(baseUrl, QStringLiteral("public-anon-key"),
        directory.filePath(QStringLiteral("refresh-token.bin")), weatherEndpoints);
    QVERIFY(controller.signIn(QStringLiteral("person@example.com"), QStringLiteral("correct-horse")));
    QTRY_COMPARE_WITH_TIMEOUT(state.geocodingRequests, 1, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(state.pendingGeocodingResponses.size(), 1, 5000);
    controller.reload();
    QTRY_COMPARE_WITH_TIMEOUT(state.profileRequests, 2, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(state.roomsRequests, 2, 5000);
    QCOMPARE(state.geocodingRequests, 1);

    writeResponse(state.pendingGeocodingResponses.takeFirst());
    QTRY_COMPARE_WITH_TIMEOUT(state.forecastRequests, 1, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(controller.weatherLine(), QStringLiteral("Partly cloudy · 20.9 °C"), 5000);
    state.deferGeocodingResponses = false;

    QVERIFY(state.geocodingRequest.contains(QByteArrayLiteral("name=Nairobi")));
    QVERIFY(state.forecastRequest.contains(QByteArrayLiteral("latitude=-1.290000")));
    QVERIFY(state.forecastRequest.contains(QByteArrayLiteral("longitude=36.820000")));
    QVERIFY(!state.geocodingRequest.toLower().contains(QByteArrayLiteral("authorization:")));
    QVERIFY(!state.forecastRequest.toLower().contains(QByteArrayLiteral("authorization:")));
    QCOMPARE(controller.weatherIcon(), QStringLiteral("qrc:/qt/qml/SmartHome/assets/icons/sun-cloud.svg"));

    state.updatedProfileBody = QByteArrayLiteral(
        R"([{"id":"123e4567-e89b-12d3-a456-426614174000","first_name":"Amina","city":"Mombasa"}])");
    state.profileBody = state.updatedProfileBody;
    state.geocodingBody = geocodingResponse(QStringLiteral("Mombasa"), -4.05, 39.67);
    state.forecastBody = forecastResponse(25.0);
    QVERIFY(controller.saveSettings(QStringLiteral("Amina"), QStringLiteral("Mombasa")));
    QTRY_COMPARE_WITH_TIMEOUT(state.geocodingRequests, 2, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(state.forecastRequests, 2, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(controller.weatherLine().contains(QStringLiteral("25.0 °C")), 5000);
    QVERIFY(state.geocodingRequest.contains(QByteArrayLiteral("name=Mombasa")));
    QVERIFY(state.forecastRequest.contains(QByteArrayLiteral("latitude=-4.050000")));
    QVERIFY(state.forecastRequest.contains(QByteArrayLiteral("longitude=39.670000")));

    state.forecastBody = forecastResponse(24.0);
    controller.reload();
    QTRY_COMPARE_WITH_TIMEOUT(state.forecastRequests, 3, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(controller.weatherLine().contains(QStringLiteral("24.0 °C")), 5000);
    QCOMPARE(state.geocodingRequests, 2);
}

void SessionControllerTest::pollingFollowsSignInAndApplicationState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    AuthServerState state{sessionResponse()};
    startAuthServer(server, state);

    QCOMPARE(SessionController::ActiveRefreshIntervalMs, 20000);
    SessionController controller(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()),
        QStringLiteral("public-anon-key"), directory.filePath(QStringLiteral("refresh-token.bin")),
        ApiClient::WeatherEndpoints{}, 250);
    QVERIFY(setPollingState(controller, Qt::ApplicationActive));
    QTest::qWait(300);
    QCOMPARE(state.profileRequests, 0);
    QCOMPARE(state.roomsRequests, 0);

    QVERIFY(controller.signIn(QStringLiteral("person@example.com"), QStringLiteral("correct-horse")));
    QTRY_COMPARE_WITH_TIMEOUT(state.profileRequests, 1, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(state.roomsRequests, 1, 5000);
    QSignalSpy profileChanged(&controller, &SessionController::profileChanged);
    QSignalSpy roomsChanged(&controller, &SessionController::roomsChanged);
    QVERIFY(profileChanged.isValid());
    QVERIFY(roomsChanged.isValid());
    QVERIFY(setPollingState(controller, Qt::ApplicationInactive));
    const int inactiveProfileRequests = state.profileRequests;
    const int inactiveRoomsRequests = state.roomsRequests;
    QTest::qWait(350);
    QCOMPARE(state.profileRequests, inactiveProfileRequests);
    QCOMPARE(state.roomsRequests, inactiveRoomsRequests);

    QVERIFY(setPollingState(controller, Qt::ApplicationActive));
    QTRY_VERIFY_WITH_TIMEOUT(state.profileRequests > inactiveProfileRequests, 2000);
    QTRY_VERIFY_WITH_TIMEOUT(state.roomsRequests > inactiveRoomsRequests, 2000);
    QTRY_VERIFY_WITH_TIMEOUT(profileChanged.count() > 0, 2000);
    QTRY_VERIFY_WITH_TIMEOUT(roomsChanged.count() > 0, 2000);
    controller.signOut();
    const int signedOutProfileRequests = state.profileRequests;
    const int signedOutRoomsRequests = state.roomsRequests;
    QTest::qWait(350);
    QCOMPARE(state.profileRequests, signedOutProfileRequests);
    QCOMPARE(state.roomsRequests, signedOutRoomsRequests);
}

void SessionControllerTest::reloadFetchesProfileAndRooms()
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
    QTRY_COMPARE_WITH_TIMEOUT(state.profileRequests, 1, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(state.roomsRequests, 1, 5000);

    state.deferProfileResponses = true;
    state.deferRoomsResponses = true;
    state.profileBody = profileResponse(QStringLiteral("Older profile"));
    state.roomsBody = roomResponse(QStringLiteral("Older light"));
    controller.reload();
    QTRY_COMPARE_WITH_TIMEOUT(state.profileRequests, 2, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(state.roomsRequests, 2, 5000);
    state.profileBody = profileResponse(QStringLiteral("Latest profile"));
    state.roomsBody = roomResponse(QStringLiteral("Latest light"));
    controller.reload();
    QTRY_COMPARE_WITH_TIMEOUT(state.profileRequests, 3, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(state.roomsRequests, 3, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(state.pendingProfileResponses.size(), 2, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(state.pendingRoomsResponses.size(), 2, 5000);

    writeResponse(state.pendingProfileResponses.takeAt(1));
    writeResponse(state.pendingRoomsResponses.takeAt(1));
    QTRY_COMPARE_WITH_TIMEOUT(controller.firstName(), QStringLiteral("Latest profile"), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(controller.rooms().size(), 1, 5000);
    const QVariantMap room = controller.rooms().first().toMap();
    QCOMPARE(room.value(QStringLiteral("devices")).toList().first().toMap()
                 .value(QStringLiteral("name")).toString(), QStringLiteral("Latest light"));

    const DeferredResponse olderProfile = state.pendingProfileResponses.takeFirst();
    const DeferredResponse olderRooms = state.pendingRoomsResponses.takeFirst();
    writeResponse(olderProfile);
    writeResponse(olderRooms);
    QTRY_COMPARE_WITH_TIMEOUT(olderProfile.socket->state(), QAbstractSocket::UnconnectedState, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(olderRooms.socket->state(), QAbstractSocket::UnconnectedState, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(state.pendingProfileResponses.isEmpty()
        && state.pendingRoomsResponses.isEmpty(), 5000);
    QTest::qWait(50);
    QCOMPARE(controller.firstName(), QStringLiteral("Latest profile"));
    QCOMPARE(controller.rooms().first().toMap().value(QStringLiteral("devices")).toList().first().toMap()
                 .value(QStringLiteral("name")).toString(), QStringLiteral("Latest light"));
}

void SessionControllerTest::serializesSameDeviceMutations()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    AuthServerState state{sessionResponse()};
    state.roomsBody = roomResponse(QStringLiteral("Desk light"));
    startAuthServer(server, state);

    SessionController controller(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()),
        QStringLiteral("public-anon-key"), directory.filePath(QStringLiteral("refresh-token.bin")));
    QVERIFY(controller.signIn(QStringLiteral("person@example.com"), QStringLiteral("correct-horse")));
    QTRY_COMPARE_WITH_TIMEOUT(state.roomsRequests, 1, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(controller.rooms().size(), 1, 5000);
    state.deferDeviceUpdateResponses = true;
    const QString deviceId = QStringLiteral("123e4567-e89b-12d3-a456-426614174020");
    QVERIFY(controller.renameDevice(deviceId, QStringLiteral("Older name")));
    QTRY_COMPARE_WITH_TIMEOUT(state.deviceUpdateRequests, 1, 5000);
    QVERIFY(!controller.renameDevice(deviceId, QStringLiteral("Latest name")));
    QCOMPARE(state.deviceUpdateRequests, 1);
    QTRY_COMPARE_WITH_TIMEOUT(state.pendingDeviceUpdateResponses.size(), 1, 5000);

    const DeferredResponse older = state.pendingDeviceUpdateResponses.takeFirst();
    writeResponse(older);
    QTRY_COMPARE_WITH_TIMEOUT(older.socket->state(), QAbstractSocket::UnconnectedState, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(controller.rooms().first().toMap().value(QStringLiteral("devices")).toList()
                                  .first().toMap().value(QStringLiteral("name")).toString(),
        QStringLiteral("Older name"), 5000);
    state.deferDeviceUpdateResponses = false;
    QVERIFY(controller.renameDevice(deviceId, QStringLiteral("Latest name")));
    QTRY_COMPARE_WITH_TIMEOUT(state.deviceUpdateRequests, 2, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(controller.rooms().first().toMap().value(QStringLiteral("devices")).toList()
                                  .first().toMap().value(QStringLiteral("name")).toString(),
        QStringLiteral("Latest name"), 5000);
    QTest::qWait(50);
    QCOMPARE(controller.rooms().first().toMap().value(QStringLiteral("devices")).toList()
                 .first().toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Latest name"));
}

void SessionControllerTest::ignoresOlderWeatherForecastResponse()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    AuthServerState state{sessionResponse()};
    state.profileBody = QByteArrayLiteral(
        R"([{"id":"123e4567-e89b-12d3-a456-426614174000","first_name":"Amina","city":"Old City"}])");
    state.geocodingBody = geocodingResponse(QStringLiteral("Old City"), 1.0, 2.0);
    state.forecastBody = forecastResponse(12.0);
    state.deferGeocodingResponses = true;
    state.deferForecastResponses = true;
    startAuthServer(server, state);

    const QString baseUrl = QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort());
    ApiClient::WeatherEndpoints endpoints{
        QUrl(baseUrl + QStringLiteral("/geocode")), QUrl(baseUrl + QStringLiteral("/forecast"))};
    SessionController controller(baseUrl, QStringLiteral("public-anon-key"),
        directory.filePath(QStringLiteral("refresh-token.bin")), endpoints);
    QVERIFY(controller.signIn(QStringLiteral("person@example.com"), QStringLiteral("correct-horse")));
    QTRY_COMPARE_WITH_TIMEOUT(state.geocodingRequests, 1, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(state.pendingGeocodingResponses.size(), 1, 5000);

    writeResponse(state.pendingGeocodingResponses.takeFirst());
    QTRY_COMPARE_WITH_TIMEOUT(state.forecastRequests, 1, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(state.pendingForecastResponses.size(), 1, 5000);
    state.updatedProfileBody = QByteArrayLiteral(
        R"([{"id":"123e4567-e89b-12d3-a456-426614174000","first_name":"Amina","city":"New City"}])");
    state.geocodingBody = geocodingResponse(QStringLiteral("New City"), 3.0, 4.0);
    state.forecastBody = forecastResponse(25.0);
    QVERIFY(controller.saveSettings(QStringLiteral("Amina"), QStringLiteral("New City")));
    QTRY_COMPARE_WITH_TIMEOUT(state.geocodingRequests, 2, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(state.pendingGeocodingResponses.size(), 1, 5000);

    writeResponse(state.pendingGeocodingResponses.takeFirst());
    QTRY_COMPARE_WITH_TIMEOUT(state.forecastRequests, 2, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(state.pendingForecastResponses.size(), 2, 5000);
    writeResponse(state.pendingForecastResponses.takeAt(1));
    QTRY_VERIFY_WITH_TIMEOUT(controller.weatherLine().contains(QStringLiteral("25.0 °C")), 5000);
    const DeferredResponse olderForecast = state.pendingForecastResponses.takeFirst();
    writeResponse(olderForecast);
    QTRY_COMPARE_WITH_TIMEOUT(olderForecast.socket->state(), QAbstractSocket::UnconnectedState, 5000);
    QTest::qWait(50);
    QVERIFY(controller.weatherLine().contains(QStringLiteral("25.0 °C")));
}

void SessionControllerTest::ignoresWeatherResponseAfterCityCleared()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    AuthServerState state{sessionResponse()};
    state.profileBody = QByteArrayLiteral(
        R"([{"id":"123e4567-e89b-12d3-a456-426614174000","first_name":"Amina","city":"Nairobi"}])");
    state.deferForecastResponses = true;
    startAuthServer(server, state);

    const QString baseUrl = QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort());
    ApiClient::WeatherEndpoints endpoints{
        QUrl(baseUrl + QStringLiteral("/geocode")), QUrl(baseUrl + QStringLiteral("/forecast"))};
    SessionController controller(baseUrl, QStringLiteral("public-anon-key"),
        directory.filePath(QStringLiteral("refresh-token.bin")), endpoints);
    QVERIFY(controller.signIn(QStringLiteral("person@example.com"), QStringLiteral("correct-horse")));
    QTRY_COMPARE_WITH_TIMEOUT(state.forecastRequests, 1, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(state.pendingForecastResponses.size(), 1, 5000);

    state.updatedProfileBody = QByteArrayLiteral(
        R"([{"id":"123e4567-e89b-12d3-a456-426614174000","first_name":"Amina","city":null}])");
    QVERIFY(controller.saveSettings(QStringLiteral("Amina"), QString()));
    QTRY_VERIFY_WITH_TIMEOUT(controller.city().isEmpty(), 5000);
    QVERIFY(controller.weatherLine().isEmpty());

    const DeferredResponse staleForecast = state.pendingForecastResponses.takeFirst();
    writeResponse(staleForecast);
    QTRY_COMPARE_WITH_TIMEOUT(staleForecast.socket->state(), QAbstractSocket::UnconnectedState, 5000);
    QTest::qWait(50);
    QVERIFY(controller.weatherLine().isEmpty());
}

QTEST_MAIN(SessionControllerTest)
#include "tst_session_controller.moc"
