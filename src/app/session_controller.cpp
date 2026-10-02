#include "session_controller.h"

#include "home_rules.h"
#include "token_store.h"

#include <QDateTime>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTime>

#include <algorithm>

namespace {

QVariantMap deviceMap(const DeviceRow& device)
{
    QVariantMap map;
    map.insert(QStringLiteral("id"), device.id);
    map.insert(QStringLiteral("roomId"), device.roomId);
    map.insert(QStringLiteral("name"), device.name);
    map.insert(QStringLiteral("kind"), device.kind);
    map.insert(QStringLiteral("isOn"), device.isOn.has_value() ? QVariant(*device.isOn) : QVariant());
    map.insert(QStringLiteral("celsius"), device.celsius.has_value() ? QVariant(*device.celsius) : QVariant());
    map.insert(QStringLiteral("position"), device.position);
    return map;
}

bool mutationHasReturnedRow(const QByteArray& body)
{
    const QJsonDocument response = QJsonDocument::fromJson(body);
    return response.isArray() && !response.array().isEmpty() && response.array().first().isObject();
}

} // namespace

SessionController::SessionController(QObject* parent)
    : SessionController(QStringLiteral(SMARTHOME_SUPABASE_URL), QStringLiteral(SMARTHOME_SUPABASE_ANON_KEY),
          sessionFilePath(), parent)
{
}

SessionController::SessionController(const QString& apiBaseUrl, const QString& anonKey,
    const QString& tokenFilePath, QObject* parent)
    : QObject(parent)
    , api_(apiBaseUrl, anonKey, this)
    , tokenFilePath_(tokenFilePath)
{
    connect(&api_, &ApiClient::completed, this, &SessionController::onCompleted);
    connect(&api_, &ApiClient::failed, this, &SessionController::onFailed);
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState) {
        emit applicationActiveChanged();
    });

    if (apiBaseUrl.isEmpty() || anonKey.isEmpty()) {
        setStatus(QStringLiteral("Set the Supabase URL and anon key in config.local.cmake."));
        return;
    }
    refreshToken_ = loadRefreshToken(tokenFilePath_);
    if (!refreshToken_.isEmpty()) {
        startRefresh();
    }
}

QString SessionController::greeting() const
{
    const QString hello = greetingFor(QTime::currentTime());
    if (firstName_.isEmpty()) {
        return hello;
    }
    return hello + QLatin1Char(' ') + firstName_;
}

QString SessionController::weatherIcon() const
{
    if (weatherIconFile_.isEmpty()) {
        return {};
    }
    return QStringLiteral("qrc:/qt/qml/SmartHome/assets/icons/") + weatherIconFile_;
}

bool SessionController::applicationActive() const
{
    return QGuiApplication::applicationState() == Qt::ApplicationActive;
}

QVariantList SessionController::rooms() const
{
    QVariantList list;
    for (const RoomRow& room : rooms_) {
        QVariantList devices;
        for (const DeviceRow& device : room.devices) {
            devices.append(deviceMap(device));
        }
        QVariantMap map;
        map.insert(QStringLiteral("id"), room.id);
        map.insert(QStringLiteral("name"), room.name);
        map.insert(QStringLiteral("position"), room.position);
        map.insert(QStringLiteral("devices"), devices);
        list.append(map);
    }
    return list;
}

void SessionController::setStatus(const QString& message)
{
    if (statusMessage_ == message) {
        return;
    }
    statusMessage_ = message;
    emit statusChanged();
}

bool SessionController::requireFields(const QString& name, const QString& email, const QString& password, bool withName)
{
    if (withName && !isNameOk(name.trimmed())) {
        setStatus(QStringLiteral("Use 1 to 40 characters."));
        return false;
    }
    if (!isEmailOk(email)) {
        setStatus(QStringLiteral("Enter a valid email."));
        return false;
    }
    if (!isPasswordOk(password)) {
        setStatus(QStringLiteral("Use 8 to 72 characters."));
        return false;
    }
    return true;
}

bool SessionController::signIn(const QString& email, const QString& password)
{
    const QString normalized = email.trimmed().toLower();
    if (!requireFields(QString(), normalized, password, false)) {
        return false;
    }
    cancelPendingAuth();
    setStatus({});
    api_.signIn(normalized, password);
    return true;
}

bool SessionController::registerAccount(const QString& firstName, const QString& email, const QString& password)
{
    const QString name = firstName.trimmed();
    const QString normalized = email.trimmed().toLower();
    if (!requireFields(name, normalized, password, true)) {
        return false;
    }
    cancelPendingAuth();
    setStatus({});
    api_.signUp(normalized, password, name);
    return true;
}

void SessionController::signOut()
{
    api_.cancelPendingRequests();
    if (!accessToken_.isEmpty()) {
        api_.logOut();
    }
    if (clearLocal()) {
        setStatus({});
    } else {
        setStatus(QStringLiteral("Could not remove the saved session. It may still be usable on this device."));
    }
}

void SessionController::reload()
{
    if (!signedIn_) {
        return;
    }
    emit greetingChanged();
    authed([this]() {
        api_.fetchProfile();
        api_.fetchRooms();
    });
}

bool SessionController::createRoom(const QString& name)
{
    const QString trimmed = name.trimmed();
    if (!isNameOk(trimmed)) {
        setStatus(QStringLiteral("Use 1 to 40 characters."));
        return false;
    }
    if (!beginMutation(QStringLiteral("room-insert"))) {
        return false;
    }
    setStatus({});
    const int position = nextRoomPosition();
    authed([this, trimmed, position]() { api_.insertRoom(trimmed, position); });
    return true;
}

bool SessionController::renameRoom(const QString& id, const QString& name)
{
    const QString trimmed = name.trimmed();
    if (!isNameOk(trimmed)) {
        setStatus(QStringLiteral("Use 1 to 40 characters."));
        return false;
    }
    if (!beginMutation(QStringLiteral("room-update"))) {
        return false;
    }
    setStatus({});
    authed([this, id, trimmed]() { api_.updateRoom(id, trimmed); });
    return true;
}

void SessionController::deleteRoom(const QString& id)
{
    if (!beginMutation(QStringLiteral("room-delete"))) {
        return;
    }
    setStatus({});
    authed([this, id]() { api_.deleteRoom(id); });
}

bool SessionController::createDevice(const QString& roomId, const QString& name, const QString& kind)
{
    const QString trimmed = name.trimmed();
    if (!isNameOk(trimmed)) {
        setStatus(QStringLiteral("Use 1 to 40 characters."));
        return false;
    }
    if (!isDeviceKindCreatable(kind)) {
        setStatus(QStringLiteral("The service could not complete the request."));
        return false;
    }
    if (!beginMutation(QStringLiteral("device-insert"))) {
        return false;
    }
    setStatus({});
    const int position = nextDevicePosition(roomId);
    authed([this, roomId, trimmed, kind, position]() { api_.insertDevice(roomId, trimmed, kind, position); });
    return true;
}

bool SessionController::renameDevice(const QString& id, const QString& name)
{
    const QString trimmed = name.trimmed();
    if (!isNameOk(trimmed)) {
        setStatus(QStringLiteral("Use 1 to 40 characters."));
        return false;
    }
    if (!beginMutation(QStringLiteral("device-update"))) {
        return false;
    }
    setStatus({});
    authed([this, id, trimmed]() { api_.updateDeviceName(id, trimmed); });
    return true;
}

void SessionController::setDeviceOn(const QString& id, bool on)
{
    std::optional<bool> previous;
    for (RoomRow& room : rooms_) {
        for (DeviceRow& device : room.devices) {
            if (device.id != id || device.kind == QLatin1String("thermometer")) {
                continue;
            }
            previous = device.isOn;
            break;
        }
        if (previous.has_value()) {
            break;
        }
    }
    if (!previous.has_value() || !beginMutation(QStringLiteral("device-on"))) {
        return;
    }
    for (RoomRow& room : rooms_) {
        for (DeviceRow& device : room.devices) {
            if (device.id == id) {
                device.isOn = on;
            }
        }
    }
    emit roomsChanged();
    setStatus({});
    toggleRestoreId_ = id;
    toggleRestoreOn_ = previous;
    authed([this, id, on]() { api_.setDeviceOn(id, on); });
}

void SessionController::deleteDevice(const QString& id)
{
    if (!beginMutation(QStringLiteral("device-delete"))) {
        return;
    }
    setStatus({});
    authed([this, id]() { api_.deleteDevice(id); });
}

bool SessionController::saveSettings(const QString& firstName, const QString& city)
{
    const QString name = firstName.trimmed();
    const QString place = city.trimmed();
    if (!isNameOk(name)) {
        setStatus(QStringLiteral("Use 1 to 40 characters."));
        return false;
    }
    if (!isCityOk(place)) {
        setStatus(QStringLiteral("Use at most 80 characters."));
        return false;
    }
    if (!beginMutation(QStringLiteral("profile-update"))) {
        return false;
    }
    setStatus({});
    authed([this, name, place]() { api_.updateProfile(userId_, name, place); });
    return true;
}

void SessionController::onCompleted(const QString& op, quint64 requestId, int status, const QByteArray& body)
{
    api_.discardRequest(requestId);
    Q_UNUSED(status)
    if (op == QLatin1String("signup") || op == QLatin1String("login") || op == QLatin1String("refresh")) {
        const std::optional<SessionTokens> session = parseSession(body);
        if (!session.has_value()) {
            setStatus(QStringLiteral("The service could not complete the request."));
            if (op == QLatin1String("refresh")) {
                failRefresh(QStringLiteral("The service could not complete the request."));
            }
            return;
        }
        applySession(*session);
        return;
    }
    if (op == QLatin1String("profile") || op == QLatin1String("profile-update")) {
        const std::optional<ProfileRow> profile = parseProfile(body);
        if (profile.has_value()) {
            applyProfile(*profile);
        } else if (op == QLatin1String("profile-update")) {
            setStatus(QStringLiteral("The service could not complete the request."));
        }
        finishMutation(op, profile.has_value());
        return;
    }
    if (op == QLatin1String("rooms")) {
        const std::optional<QList<RoomRow>> parsed = parseRooms(body);
        if (!parsed.has_value()) {
            setStatus(QStringLiteral("The service could not complete the request."));
            return;
        }
        rooms_ = *parsed;
        emit roomsChanged();
        return;
    }
    if (op == QLatin1String("geocode")) {
        const std::optional<GeoHit> hit = parseGeocoding(body);
        if (!hit.has_value()) {
            setStatus(QStringLiteral("Couldn't find that city."));
            return;
        }
        api_.forecast(hit->latitude, hit->longitude);
        return;
    }
    if (op == QLatin1String("forecast")) {
        const std::optional<ForecastNow> now = parseForecast(body);
        if (!now.has_value()) {
            return;
        }
        weatherLine_ = QString::number(now->temperatureCelsius, 'f', 1) + QStringLiteral("°  ")
            + weatherLabel(now->weatherCode);
        weatherIconFile_ = ::weatherIconFile(now->weatherCode, now->isDay);
        emit weatherChanged();
        return;
    }
    if (op == QLatin1String("logout")) {
        return;
    }
    if ((op == QLatin1String("room-insert") || op == QLatin1String("room-update")
            || op == QLatin1String("room-delete") || op == QLatin1String("device-insert")
            || op == QLatin1String("device-on") || op == QLatin1String("device-update")
            || op == QLatin1String("device-delete"))
        && !mutationHasReturnedRow(body)) {
        setStatus(QStringLiteral("The service could not complete the request."));
        finishMutation(op, false);
        return;
    }
    if (op == QLatin1String("device-on")) {
        toggleRestoreId_.clear();
    }
    setStatus({});
    api_.fetchRooms();
    finishMutation(op, true);
}

void SessionController::onFailed(const QString& op, quint64 requestId, int status, const QString& message)
{
    if (status == 401 && op != QLatin1String("login") && op != QLatin1String("signup")
        && op != QLatin1String("refresh") && op != QLatin1String("logout")
        && api_.queueRetry(requestId)) {
        startRefresh();
        return;
    }
    api_.discardRequest(requestId);
    if (op == QLatin1String("forecast")) {
        return;
    }
    if (op == QLatin1String("geocode")) {
        setStatus(QStringLiteral("Couldn't find that city."));
        return;
    }
    if (op == QLatin1String("logout")) {
        return;
    }
    if (op == QLatin1String("refresh")) {
        if (status == 400 || status == 401) {
            refreshRunning_ = false;
            afterRefresh_.clear();
            api_.cancelPendingRequests();
            setStatus(clearLocal()
                    ? message
                    : message + QStringLiteral(" The saved session could not be removed from this device."));
            return;
        }
        failRefresh(message);
        return;
    }
    if (op == QLatin1String("device-on") && !toggleRestoreId_.isEmpty()) {
        rollbackDeviceToggle();
        setStatus(QStringLiteral("Couldn't update the device."));
        finishMutation(op, false);
        return;
    }
    setStatus(message);
    finishMutation(op, false);
}

void SessionController::applySession(const SessionTokens& session)
{
    accessToken_ = session.accessToken;
    api_.setAccessToken(accessToken_);
    refreshToken_ = session.refreshToken;
    userId_ = session.userId;
    if (!saveRefreshToken(refreshToken_, tokenFilePath_)) {
        setStatus(QStringLiteral("Session active, but it couldn't be saved. You may need to sign in again after closing the app."));
    }
    refreshRunning_ = false;
    api_.retryQueuedRequests();
    if (!signedIn_) {
        signedIn_ = true;
        emit signedInChanged();
    }
    QList<std::function<void()>> queued;
    queued.swap(afterRefresh_);
    if (queued.isEmpty()) {
        api_.fetchProfile();
        api_.fetchRooms();
    } else {
        for (const std::function<void()>& call : queued) {
            call();
        }
    }
}

void SessionController::applyProfile(const ProfileRow& profile)
{
    userId_ = profile.id;
    const bool nameChanged = firstName_ != profile.firstName;
    const bool cityChanged = city_ != profile.city;
    firstName_ = profile.firstName;
    city_ = profile.city;
    emit profileChanged();
    if (nameChanged) {
        emit greetingChanged();
    }
    if (cityChanged) {
        if (city_.isEmpty()) {
            weatherLine_.clear();
            weatherIconFile_.clear();
            emit weatherChanged();
        } else {
            api_.geocode(city_);
        }
    }
}

void SessionController::authed(const std::function<void()>& call)
{
    const QDateTime expiry = jwtExpiryUtc(accessToken_);
    const bool due = accessToken_.isEmpty() || !expiry.isValid()
        || expiry <= QDateTime::currentDateTimeUtc().addSecs(60);
    if (!due) {
        call();
        return;
    }
    afterRefresh_.append(call);
    startRefresh();
}

void SessionController::startRefresh()
{
    if (refreshRunning_) {
        return;
    }
    if (refreshToken_.isEmpty()) {
        api_.cancelPendingRequests();
        clearLocal();
        setStatus(QStringLiteral("The service could not complete the request."));
        return;
    }
    refreshRunning_ = true;
    api_.refresh(refreshToken_);
}

void SessionController::cancelPendingAuth()
{
    api_.cancelPendingRequests();
    afterRefresh_.clear();
    refreshRunning_ = false;
}

void SessionController::failRefresh(const QString& message)
{
    refreshRunning_ = false;
    afterRefresh_.clear();
    api_.cancelPendingRequests();
    failPendingMutation();
    setStatus(message);
}

void SessionController::failPendingMutation()
{
    const QString op = pendingMutation_;
    if (op.isEmpty()) {
        return;
    }
    if (op == QLatin1String("device-on")) {
        rollbackDeviceToggle();
    }
    finishMutation(op, false);
}

void SessionController::rollbackDeviceToggle()
{
    if (toggleRestoreId_.isEmpty()) {
        return;
    }
    for (RoomRow& room : rooms_) {
        for (DeviceRow& device : room.devices) {
            if (device.id == toggleRestoreId_) {
                device.isOn = toggleRestoreOn_;
            }
        }
    }
    toggleRestoreId_.clear();
    emit roomsChanged();
}

bool SessionController::clearLocal()
{
    failPendingMutation();
    accessToken_.clear();
    refreshToken_.clear();
    api_.setAccessToken({});
    const bool tokenRemoved = clearRefreshToken(tokenFilePath_);
    userId_.clear();
    firstName_.clear();
    city_.clear();
    rooms_.clear();
    afterRefresh_.clear();
    weatherLine_.clear();
    weatherIconFile_.clear();
    toggleRestoreId_.clear();
    refreshRunning_ = false;
    const bool wasSignedIn = signedIn_;
    signedIn_ = false;
    if (wasSignedIn) {
        emit signedInChanged();
    }
    emit profileChanged();
    emit roomsChanged();
    emit weatherChanged();
    emit greetingChanged();
    return tokenRemoved;
}

bool SessionController::beginMutation(const QString& op)
{
    if (!pendingMutation_.isEmpty()) {
        return false;
    }
    pendingMutation_ = op;
    emit mutationStateChanged();
    return true;
}

void SessionController::finishMutation(const QString& op, bool success)
{
    if (pendingMutation_ != op) {
        return;
    }
    pendingMutation_.clear();
    emit mutationStateChanged();
    emit mutationFinished(op, success);
}

int SessionController::nextRoomPosition() const
{
    int maxPosition = -1;
    for (const RoomRow& room : rooms_) {
        maxPosition = std::max(maxPosition, room.position);
    }
    return maxPosition + 1;
}

int SessionController::nextDevicePosition(const QString& roomId) const
{
    int maxPosition = -1;
    for (const RoomRow& room : rooms_) {
        if (room.id != roomId) {
            continue;
        }
        for (const DeviceRow& device : room.devices) {
            maxPosition = std::max(maxPosition, device.position);
        }
    }
    return maxPosition + 1;
}
