#include "session_controller.h"

#include "home_json.h"
#include "home_rules.h"
#include "token_store.h"

#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QTime>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

#include <algorithm>
#include <utility>

SessionController::SessionController(QObject* parent)
    : SessionController(QStringLiteral(SMARTHOME_SUPABASE_URL), QStringLiteral(SMARTHOME_SUPABASE_ANON_KEY),
          sessionFilePath(), parent)
{
}

SessionController::SessionController(QString baseUrl, QString anonKey, QString tokenFilePath, QObject* parent)
    : SessionController(std::move(baseUrl), std::move(anonKey), std::move(tokenFilePath),
          ApiClient::WeatherEndpoints{}, parent)
{
}

SessionController::SessionController(QString baseUrl, QString anonKey, QString tokenFilePath,
    ApiClient::WeatherEndpoints weatherEndpoints, QObject* parent)
    : SessionController(std::move(baseUrl), std::move(anonKey), std::move(tokenFilePath),
          std::move(weatherEndpoints), ActiveRefreshIntervalMs, parent)
{
}

SessionController::SessionController(QString baseUrl, QString anonKey, QString tokenFilePath,
    ApiClient::WeatherEndpoints weatherEndpoints, int activeRefreshIntervalMs, QObject* parent)
    : QObject(parent)
    , api_(std::move(baseUrl), std::move(anonKey), std::move(weatherEndpoints))
    , tokenFilePath_(std::move(tokenFilePath))
{
    connect(&api_, &ApiClient::completed, this, &SessionController::onCompleted);
    connect(&api_, &ApiClient::failed, this, &SessionController::onFailed);
    connect(&api_, &ApiClient::authenticationRequired, this, &SessionController::onAuthenticationRequired);
    connect(&api_, &ApiClient::requestStarted, this,
        [this](const QString& op, quint64 requestId, const QString& resourceId) {
            if (op == QLatin1String("profile")) {
                latestProfileRequestId_ = requestId;
                profileRequestGenerations_.insert(requestId, profileMutationGeneration_);
                profileRequestsDuringMutation_.insert(requestId, !pendingProfileMutations_.isEmpty());
            } else if (op == QLatin1String("profile-update")) {
                latestProfileUpdateRequestId_ = requestId;
                ++profileMutationGeneration_;
                pendingProfileMutations_.insert(requestId);
            } else if (op == QLatin1String("rooms")) {
                latestRoomsRequestId_ = requestId;
                roomRequestGenerations_.insert(requestId, homeMutationGeneration_);
                roomRequestsDuringMutation_.insert(requestId, !pendingHomeMutations_.isEmpty());
            } else if (op == QLatin1String("geocode") || op == QLatin1String("forecast")) {
                latestWeatherRequestId_ = requestId;
            } else if (op.startsWith(QLatin1String("room-"))
                || op.startsWith(QLatin1String("device-"))) {
                ++homeMutationGeneration_;
                pendingHomeMutations_.insert(requestId);
                if (!resourceId.isEmpty()) {
                    const QString prefix = op.startsWith(QLatin1String("room-"))
                        ? QStringLiteral("room:") : QStringLiteral("device:");
                    const QString key = prefix + resourceId;
                    entityKeysByRequestId_.insert(requestId, key);
                    latestEntityRequestIds_.insert(key, requestId);
                }
            }
        });
    connect(&refreshTimer_, &QTimer::timeout, this, &SessionController::refreshIfNeeded);
    connect(&activeRefreshTimer_, &QTimer::timeout, this, &SessionController::reload);
    refreshTimer_.setInterval(15000);
    refreshTimer_.start();
    activeRefreshTimer_.setInterval(activeRefreshIntervalMs);
    if (auto* guiApp = qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
        connect(guiApp, &QGuiApplication::applicationStateChanged, this, &SessionController::updatePolling);
        updatePolling(guiApp->applicationState());
    }
    refreshToken_ = loadRefreshToken(tokenFilePath_);
    if (!refreshToken_.isEmpty()) {
        startRefresh();
    }
}

QString SessionController::weatherIcon() const
{
    if (weatherIconFile_.isEmpty()) {
        return {};
    }
    return QStringLiteral("qrc:/qt/qml/SmartHome/assets/icons/") + weatherIconFile_;
}

QString SessionController::greeting() const
{
    const QString band = greetingFor(QTime::currentTime());
    QString salutation;
    if (band == QLatin1String("Good morning,")) {
        salutation = tr("Good morning,");
    } else if (band == QLatin1String("Good afternoon,")) {
        salutation = tr("Good afternoon,");
    } else if (band == QLatin1String("Good evening,")) {
        salutation = tr("Good evening,");
    } else {
        salutation = tr("Good night,");
    }
    return firstName_.isEmpty() ? salutation : salutation + QLatin1Char(' ') + firstName_;
}

bool SessionController::signIn(const QString& email, const QString& password)
{
    const QString normalizedEmail = email.trimmed().toLower();
    if (!isEmailOk(normalizedEmail)) {
        setStatus(tr("Enter a valid email."));
        return false;
    }
    if (!isPasswordOk(password)) {
        setStatus(tr("Use 8 to 72 characters."));
        return false;
    }
    setStatus({});
    api_.signIn(normalizedEmail, password);
    return true;
}

bool SessionController::registerAccount(const QString& firstName, const QString& email, const QString& password)
{
    const QString normalizedName = firstName.trimmed();
    const QString normalizedEmail = email.trimmed().toLower();
    if (!isNameOk(normalizedName)) {
        setStatus(tr("Use 1 to 40 characters for the name."));
        return false;
    }
    if (!isEmailOk(normalizedEmail)) {
        setStatus(tr("Enter a valid email."));
        return false;
    }
    if (!isPasswordOk(password)) {
        setStatus(tr("Use 8 to 72 characters."));
        return false;
    }
    setStatus({});
    api_.signUp(normalizedEmail, password, normalizedName);
    return true;
}

void SessionController::signOut()
{
    api_.cancelPendingRequests();
    refreshInFlight_ = false;
    if (!accessToken_.isEmpty()) {
        api_.logOut();
    }
    const bool tokenCleared = clearRefreshToken(tokenFilePath_);
    clearLocal();
    setStatus(tokenCleared ? QString() : tr("Could not clear the session."));
}

void SessionController::reload()
{
    if (!signedIn_) {
        return;
    }
    api_.fetchProfile();
    api_.fetchRooms();
    updateWeather();
}

bool SessionController::saveSettings(const QString& firstName, const QString& city)
{
    if (!signedIn_) {
        setStatus(tr("Sign in to manage your profile."));
        return false;
    }
    const QString normalizedName = firstName.trimmed();
    const QString normalizedCity = city.trimmed();
    if (!isNameOk(normalizedName)) {
        setStatus(tr("Use 1 to 40 characters for the name."));
        return false;
    }
    if (!isCityOk(normalizedCity)) {
        setStatus(tr("Use up to 80 characters for the city."));
        return false;
    }
    setStatus({});
    api_.updateProfile(userId_, normalizedName, normalizedCity);
    return true;
}

bool SessionController::createRoom(const QString& name)
{
    if (!signedIn_) {
        setStatus(tr("Sign in to manage your home."));
        return false;
    }
    const QString normalizedName = name.trimmed();
    if (!isNameOk(normalizedName)) {
        setStatus(tr("Use 1 to 40 characters for the room name."));
        return false;
    }
    setStatus({});
    api_.insertRoom(normalizedName, nextRoomPosition());
    return true;
}

bool SessionController::renameRoom(const QString& roomId, const QString& name)
{
    if (!signedIn_) {
        setStatus(tr("Sign in to manage your home."));
        return false;
    }
    if (!isUuid(roomId)) {
        setStatus(tr("Choose an existing room."));
        return false;
    }
    const auto room = std::find_if(roomRows_.cbegin(), roomRows_.cend(), [&roomId](const RoomRow& row) {
        return row.id == roomId;
    });
    if (room == roomRows_.cend()) {
        setStatus(tr("Choose an existing room."));
        return false;
    }
    const QString normalizedName = name.trimmed();
    if (!isNameOk(normalizedName)) {
        setStatus(tr("Use 1 to 40 characters for the room name."));
        return false;
    }
    setStatus({});
    api_.updateRoom(roomId, normalizedName);
    return true;
}

bool SessionController::deleteRoom(const QString& roomId)
{
    if (!signedIn_) {
        setStatus(tr("Sign in to manage your home."));
        return false;
    }
    if (!isUuid(roomId)) {
        setStatus(tr("Choose an existing room."));
        return false;
    }
    const bool exists = std::any_of(roomRows_.cbegin(), roomRows_.cend(), [&roomId](const RoomRow& room) {
        return room.id == roomId;
    });
    if (!exists) {
        setStatus(tr("Choose an existing room."));
        return false;
    }
    setStatus({});
    api_.deleteRoom(roomId);
    return true;
}

bool SessionController::createDevice(const QString& roomId, const QString& name, const QString& kind)
{
    if (!signedIn_) {
        setStatus(tr("Sign in to manage your home."));
        return false;
    }
    const QString normalizedName = name.trimmed();
    if (!isNameOk(normalizedName)) {
        setStatus(tr("Use 1 to 40 characters for the device name."));
        return false;
    }
    if (kind != QLatin1String("light") && kind != QLatin1String("plug")
        && kind != QLatin1String("thermometer")) {
        setStatus(tr("Choose a supported device type."));
        return false;
    }
    const auto room = std::find_if(roomRows_.cbegin(), roomRows_.cend(), [&roomId](const RoomRow& row) {
        return row.id == roomId;
    });
    if (room == roomRows_.cend()) {
        setStatus(tr("Choose an existing room."));
        return false;
    }
    setStatus({});
    api_.insertDevice(roomId, normalizedName, kind, nextDevicePosition(roomId));
    return true;
}

bool SessionController::setDeviceOn(const QString& deviceId, bool on)
{
    if (!signedIn_) {
        setStatus(tr("Sign in to manage your home."));
        return false;
    }
    if (!pendingDeviceOnId_.isEmpty()) {
        setStatus(tr("Wait for the current device update to finish."));
        return false;
    }
    if (!isUuid(deviceId)) {
        setStatus(tr("Choose an existing switchable device."));
        return false;
    }
    for (RoomRow& room : roomRows_) {
        for (DeviceRow& device : room.devices) {
            if (device.id != deviceId) {
                continue;
            }
            if (!device.isOn.has_value()) {
                setStatus(tr("Thermometers cannot be switched."));
                return false;
            }
            if (*device.isOn == on) {
                return true;
            }
            pendingDeviceOnId_ = device.id;
            previousDeviceOn_ = device.isOn;
            device.isOn = on;
            emit roomsChanged();
            setStatus({});
            api_.setDeviceOn(device.id, on);
            return true;
        }
    }
    setStatus(tr("Choose an existing switchable device."));
    return false;
}

bool SessionController::renameDevice(const QString& deviceId, const QString& name)
{
    if (!signedIn_) {
        setStatus(tr("Sign in to manage your home."));
        return false;
    }
    if (!isUuid(deviceId)) {
        setStatus(tr("Choose an existing device."));
        return false;
    }
    const QString normalizedName = name.trimmed();
    if (!isNameOk(normalizedName)) {
        setStatus(tr("Use 1 to 40 characters for the device name."));
        return false;
    }
    const bool exists = std::any_of(roomRows_.cbegin(), roomRows_.cend(), [&deviceId](const RoomRow& room) {
        return std::any_of(room.devices.cbegin(), room.devices.cend(), [&deviceId](const DeviceRow& device) {
            return device.id == deviceId;
        });
    });
    if (!exists) {
        setStatus(tr("Choose an existing device."));
        return false;
    }
    setStatus({});
    api_.updateDeviceName(deviceId, normalizedName);
    return true;
}

bool SessionController::deleteDevice(const QString& deviceId)
{
    if (!signedIn_) {
        setStatus(tr("Sign in to manage your home."));
        return false;
    }
    if (!isUuid(deviceId)) {
        setStatus(tr("Choose an existing device."));
        return false;
    }
    const bool exists = std::any_of(roomRows_.cbegin(), roomRows_.cend(), [&deviceId](const RoomRow& room) {
        return std::any_of(room.devices.cbegin(), room.devices.cend(), [&deviceId](const DeviceRow& device) {
            return device.id == deviceId;
        });
    });
    if (!exists) {
        setStatus(tr("Choose an existing device."));
        return false;
    }
    setStatus({});
    api_.deleteDevice(deviceId);
    return true;
}

QVariantList SessionController::rooms() const
{
    QVariantList result;
    for (const RoomRow& room : roomRows_) {
        QVariantMap roomMap;
        roomMap.insert(QStringLiteral("id"), room.id);
        roomMap.insert(QStringLiteral("name"), room.name);
        roomMap.insert(QStringLiteral("position"), room.position);
        roomMap.insert(QStringLiteral("roomId"), room.id);
        roomMap.insert(QStringLiteral("deviceCount"), room.devices.size());
        QVariantList devices;
        for (const DeviceRow& device : room.devices) {
            QVariantMap deviceMap;
            deviceMap.insert(QStringLiteral("deviceId"), device.id);
            deviceMap.insert(QStringLiteral("name"), device.name);
            deviceMap.insert(QStringLiteral("kind"), device.kind);
            deviceMap.insert(QStringLiteral("celsius"), device.celsius.has_value()
                    ? QVariant(*device.celsius) : QVariant());
            deviceMap.insert(QStringLiteral("isOn"), device.isOn.has_value()
                    ? QVariant(*device.isOn) : QVariant());
            devices.append(deviceMap);
        }
        roomMap.insert(QStringLiteral("devices"), devices);
        result.append(roomMap);
    }
    return result;
}

void SessionController::onCompleted(const QString& op, quint64 requestId, int, const QByteArray& body)
{
    if (!isCurrentResponse(op, requestId)) {
        const QString entityKey = entityKeysByRequestId_.value(requestId);
        if (op == QLatin1String("device-on")
            && entityKey == QStringLiteral("device:") + pendingDeviceOnId_) {
            clearPendingDeviceToggle(false);
        } else if (op == QLatin1String("device-reading")
            && entityKey == QStringLiteral("device:") + pendingReadingId_) {
            pendingReadingId_.clear();
        }
        finishTrackedResponse(requestId);
        return;
    }
    finishTrackedResponse(requestId);
    if (op == QLatin1String("geocode")) {
        const std::optional<GeoHit> hit = parseGeocoding(body);
        if (!hit.has_value()) {
            weatherLine_ = tr("Couldn't find that city.");
            weatherIconFile_.clear();
            emit weatherChanged();
            return;
        }
        api_.forecast(hit->latitude, hit->longitude);
        return;
    }
    if (op == QLatin1String("forecast")) {
        const std::optional<ForecastNow> forecast = parseForecast(body);
        if (!forecast.has_value()) {
            weatherLine_ = tr("Weather unavailable.");
            weatherIconFile_.clear();
        } else {
            weatherLine_ = tr("%1 · %2 °C").arg(weatherLabel(forecast->weatherCode))
                .arg(forecast->temperatureCelsius, 0, 'f', 1);
            weatherIconFile_ = weatherIconFile(forecast->weatherCode, forecast->isDay);
        }
        emit weatherChanged();
        return;
    }
    if (op == QLatin1String("rooms")) {
        const std::optional<QList<RoomRow>> rooms = parseRooms(body);
        if (!rooms.has_value()) {
            setStatus(tr("The service could not complete the request."));
            return;
        }
        roomRows_ = *rooms;
        emit roomsChanged();
        setStatus({});
        walkStaleReadings();
        return;
    }
    if (op == QLatin1String("room-insert")) {
        const std::optional<QList<RoomRow>> rooms = parseRooms(body);
        if (!rooms.has_value() || rooms->size() != 1) {
            setStatus(tr("The service could not complete the request."));
            return;
        }
        roomRows_.append(rooms->first());
        emit roomsChanged();
        setStatus({});
        return;
    }
    if (op == QLatin1String("room-update")) {
        const std::optional<QList<RoomRow>> rooms = parseRooms(body);
        if (!rooms.has_value() || rooms->size() != 1) {
            setStatus(tr("The service could not complete the request."));
            return;
        }
        const RoomRow& updated = rooms->first();
        for (RoomRow& room : roomRows_) {
            if (room.id == updated.id) {
                const QList<DeviceRow> devices = room.devices;
                room = updated;
                room.devices = devices;
                emit roomsChanged();
                setStatus({});
                return;
            }
        }
        setStatus(tr("The service could not complete the request."));
        return;
    }
    if (op == QLatin1String("room-delete")) {
        const QJsonDocument response = QJsonDocument::fromJson(body);
        if (!response.isArray() || response.array().isEmpty() || !response.array().first().isObject()) {
            setStatus(tr("The service could not complete the request."));
            return;
        }
        const QJsonValue idValue = response.array().first().toObject().value(QStringLiteral("id"));
        if (!idValue.isString() || !isUuid(idValue.toString())) {
            setStatus(tr("The service could not complete the request."));
            return;
        }
        const QString deletedId = idValue.toString();
        const auto room = std::find_if(roomRows_.begin(), roomRows_.end(), [&deletedId](const RoomRow& row) {
            return row.id == deletedId;
        });
        if (room == roomRows_.end()) {
            setStatus(tr("The service could not complete the request."));
            return;
        }
        roomRows_.erase(room);
        emit roomsChanged();
        setStatus({});
        return;
    }
    if (op == QLatin1String("device-insert")) {
        const std::optional<QList<DeviceRow>> devices = parseDevices(body);
        if (!devices.has_value() || devices->size() != 1) {
            setStatus(tr("The service could not complete the request."));
            return;
        }
        const DeviceRow& inserted = devices->first();
        for (RoomRow& room : roomRows_) {
            if (room.id == inserted.roomId) {
                room.devices.append(inserted);
                emit roomsChanged();
                setStatus({});
                return;
            }
        }
        setStatus(tr("The service could not complete the request."));
        return;
    }
    if (op == QLatin1String("device-update")) {
        const std::optional<QList<DeviceRow>> devices = parseDevices(body);
        if (!devices.has_value() || devices->size() != 1) {
            setStatus(tr("The service could not complete the request."));
            return;
        }
        const DeviceRow& updated = devices->first();
        for (RoomRow& room : roomRows_) {
            for (DeviceRow& device : room.devices) {
                if (device.id == updated.id) {
                    device = updated;
                    emit roomsChanged();
                    setStatus({});
                    return;
                }
            }
        }
        setStatus(tr("The service could not complete the request."));
        return;
    }
    if (op == QLatin1String("device-delete")) {
        const QJsonDocument response = QJsonDocument::fromJson(body);
        if (!response.isArray() || response.array().isEmpty() || !response.array().first().isObject()) {
            setStatus(tr("The service could not complete the request."));
            return;
        }
        const QJsonValue idValue = response.array().first().toObject().value(QStringLiteral("id"));
        if (!idValue.isString() || !isUuid(idValue.toString())) {
            setStatus(tr("The service could not complete the request."));
            return;
        }
        const QString deletedId = idValue.toString();
        for (RoomRow& room : roomRows_) {
            const auto device = std::find_if(room.devices.begin(), room.devices.end(), [&deletedId](const DeviceRow& row) {
                return row.id == deletedId;
            });
            if (device != room.devices.end()) {
                room.devices.erase(device);
                emit roomsChanged();
                setStatus({});
                return;
            }
        }
        setStatus(tr("The service could not complete the request."));
        return;
    }
    if (op == QLatin1String("device-on")) {
        const std::optional<QList<DeviceRow>> devices = parseDevices(body);
        if (!devices.has_value() || devices->size() != 1
            || devices->first().id != pendingDeviceOnId_ || !devices->first().isOn.has_value()) {
            clearPendingDeviceToggle(true);
            setStatus(tr("Couldn't update the device."));
            return;
        }
        const DeviceRow updated = devices->first();
        for (RoomRow& room : roomRows_) {
            for (DeviceRow& device : room.devices) {
                if (device.id == updated.id) {
                    device = updated;
                    clearPendingDeviceToggle(false);
                    emit roomsChanged();
                    setStatus({});
                    return;
                }
            }
        }
        clearPendingDeviceToggle(true);
        setStatus(tr("Couldn't update the device."));
        return;
    }
    if (op == QLatin1String("device-reading")) {
        const std::optional<QList<DeviceRow>> devices = parseDevices(body);
        if (!devices.has_value() || devices->size() != 1
            || devices->first().id != pendingReadingId_ || !devices->first().celsius.has_value()) {
            pendingReadingId_.clear();
            setStatus(tr("Couldn't update the thermometer."));
            return;
        }
        const DeviceRow updated = devices->first();
        for (RoomRow& room : roomRows_) {
            for (DeviceRow& device : room.devices) {
                if (device.id == updated.id) {
                    device = updated;
                    pendingReadingId_.clear();
                    emit roomsChanged();
                    setStatus({});
                    return;
                }
            }
        }
        pendingReadingId_.clear();
        setStatus(tr("Couldn't update the thermometer."));
        return;
    }
    if (op == QLatin1String("profile") || op == QLatin1String("profile-update")) {
        const std::optional<ProfileRow> profile = parseProfile(body);
        if (!profile.has_value() || profile->id != userId_) {
            setStatus(tr("The service could not complete the request."));
            return;
        }
        const bool cityChanged = city_ != profile->city;
        userId_ = profile->id;
        firstName_ = profile->firstName;
        city_ = profile->city;
        emit profileChanged();
        if (cityChanged) {
            updateWeather();
        }
        setStatus({});
        return;
    }
    if (op != QLatin1String("login") && op != QLatin1String("signup") && op != QLatin1String("refresh")) {
        return;
    }
    const bool isRefresh = op == QLatin1String("refresh");
    const std::optional<SessionTokens> session = parseSession(body);
    if (!session.has_value()) {
        if (isRefresh) {
            refreshInFlight_ = false;
            clearRefreshToken(tokenFilePath_);
            clearLocal();
        }
        setStatus(tr("The service could not complete the request."));
        return;
    }
    if (isRefresh) {
        refreshInFlight_ = false;
    }
    applySession(*session, isRefresh);
}

void SessionController::onFailed(const QString& op, quint64 requestId, int, const QString& message)
{
    if (!isCurrentResponse(op, requestId)) {
        const QString entityKey = entityKeysByRequestId_.value(requestId);
        if (op == QLatin1String("device-on")
            && entityKey == QStringLiteral("device:") + pendingDeviceOnId_) {
            clearPendingDeviceToggle(true);
        } else if (op == QLatin1String("device-reading")
            && entityKey == QStringLiteral("device:") + pendingReadingId_) {
            pendingReadingId_.clear();
        }
        finishTrackedResponse(requestId);
        return;
    }
    finishTrackedResponse(requestId);
    if (op == QLatin1String("refresh")) {
        refreshInFlight_ = false;
        clearRefreshToken(tokenFilePath_);
        clearLocal();
        setStatus(message);
    } else if (op == QLatin1String("login") || op == QLatin1String("signup")
        || op == QLatin1String("profile") || op == QLatin1String("profile-update")
        || op == QLatin1String("rooms") || op == QLatin1String("room-insert")
        || op == QLatin1String("room-update") || op == QLatin1String("room-delete")
        || op == QLatin1String("device-insert")) {
        setStatus(message);
    } else if (op == QLatin1String("device-on")) {
        clearPendingDeviceToggle(true);
        setStatus(message);
    } else if (op == QLatin1String("device-reading")) {
        pendingReadingId_.clear();
        setStatus(message);
    } else if (op == QLatin1String("geocode")) {
        weatherLine_ = tr("Couldn't find that city.");
        weatherIconFile_.clear();
        emit weatherChanged();
    } else if (op == QLatin1String("forecast")) {
        weatherLine_ = tr("Weather unavailable.");
        weatherIconFile_.clear();
        emit weatherChanged();
    } else if (op == QLatin1String("device-update") || op == QLatin1String("device-delete")) {
        setStatus(message);
    }
}

void SessionController::onAuthenticationRequired(const QString&, quint64)
{
    startRefresh();
}

void SessionController::startRefresh()
{
    if (refreshInFlight_) {
        return;
    }
    if (refreshToken_.isEmpty()) {
        api_.cancelPendingRequests();
        clearLocal();
        setStatus(tr("The service could not complete the request."));
        return;
    }
    refreshInFlight_ = true;
    api_.refresh(refreshToken_);
}

void SessionController::refreshIfNeeded()
{
    if (signedIn_ && accessTokenExpiresAt_.isValid()
        && QDateTime::currentDateTimeUtc().addSecs(60) >= accessTokenExpiresAt_) {
        startRefresh();
    }
}

void SessionController::applySession(const SessionTokens& session, bool isRefresh)
{
    if (!saveRefreshToken(session.refreshToken, tokenFilePath_)) {
        if (isRefresh) {
            clearRefreshToken(tokenFilePath_);
            clearLocal();
        }
        setStatus(tr("Could not save the session."));
        return;
    }
    const bool wasSignedIn = signedIn_;
    accessToken_ = session.accessToken;
    refreshToken_ = session.refreshToken;
    userId_ = session.userId;
    accessTokenExpiresAt_ = jwtExpiryUtc(accessToken_);
    if (!accessTokenExpiresAt_.isValid() && session.expiresIn > 0) {
        accessTokenExpiresAt_ = QDateTime::currentDateTimeUtc().addSecs(session.expiresIn);
    }
    email_ = session.email;
    api_.setAccessToken(accessToken_);
    signedIn_ = true;
    if (auto* guiApp = qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
        updatePolling(guiApp->applicationState());
    }
    if (!wasSignedIn) {
        emit signedInChanged();
    }
    emit emailChanged();
    setStatus({});
    if (isRefresh) {
        api_.retryQueuedRequests();
        if (wasSignedIn) {
            return;
        }
    }
    api_.fetchProfile();
    api_.fetchRooms();
}

void SessionController::clearLocal()
{
    const bool wasSignedIn = signedIn_;
    const bool hadEmail = !email_.isEmpty();
    const bool hadProfile = !userId_.isEmpty() || !firstName_.isEmpty() || !city_.isEmpty();
    const bool hadWeather = !weatherLine_.isEmpty() || !weatherIconFile_.isEmpty();
    const bool hadRooms = !roomRows_.isEmpty();
    signedIn_ = false;
    activeRefreshTimer_.stop();
    accessToken_.clear();
    refreshToken_.clear();
    accessTokenExpiresAt_ = {};
    refreshInFlight_ = false;
    email_.clear();
    userId_.clear();
    firstName_.clear();
    city_.clear();
    weatherLine_.clear();
    weatherIconFile_.clear();
    roomRows_.clear();
    pendingDeviceOnId_.clear();
    previousDeviceOn_.reset();
    pendingReadingId_.clear();
    latestProfileRequestId_ = 0;
    latestProfileUpdateRequestId_ = 0;
    latestRoomsRequestId_ = 0;
    latestWeatherRequestId_ = 0;
    profileMutationGeneration_ = 0;
    homeMutationGeneration_ = 0;
    profileRequestGenerations_.clear();
    profileRequestsDuringMutation_.clear();
    pendingProfileMutations_.clear();
    roomRequestGenerations_.clear();
    roomRequestsDuringMutation_.clear();
    pendingHomeMutations_.clear();
    entityKeysByRequestId_.clear();
    latestEntityRequestIds_.clear();
    api_.setAccessToken({});
    if (wasSignedIn) {
        emit signedInChanged();
    }
    if (hadEmail) {
        emit emailChanged();
    }
    if (hadProfile) {
        emit profileChanged();
    }
    if (hadWeather) {
        emit weatherChanged();
    }
    if (hadRooms) {
        emit roomsChanged();
    }
}

int SessionController::nextRoomPosition() const
{
    int position = 0;
    for (const RoomRow& room : roomRows_) {
        position = qMax(position, room.position + 1);
    }
    return position;
}

int SessionController::nextDevicePosition(const QString& roomId) const
{
    int position = 0;
    for (const RoomRow& room : roomRows_) {
        if (room.id != roomId) {
            continue;
        }
        for (const DeviceRow& device : room.devices) {
            position = qMax(position, device.position + 1);
        }
        break;
    }
    return position;
}

void SessionController::walkStaleReadings()
{
    if (!signedIn_ || !pendingReadingId_.isEmpty()) {
        return;
    }
    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (const RoomRow& room : roomRows_) {
        for (const DeviceRow& device : room.devices) {
            if (device.kind != QLatin1String("thermometer") || !device.celsius.has_value()
                || !readingIsStale(device.readingAt, now)) {
                continue;
            }
            int tenths = QRandomGenerator::global()->bounded(-3, 4);
            if (tenths == 0) {
                tenths = 1;
            }
            pendingReadingId_ = device.id;
            api_.setReading(device.id, nextCelsius(*device.celsius, tenths / 10.0), now);
            return;
        }
    }
}

void SessionController::updateWeather()
{
    if (city_.isEmpty()) {
        latestWeatherRequestId_ = 0;
        weatherLine_.clear();
        weatherIconFile_.clear();
        emit weatherChanged();
        return;
    }
    api_.geocode(city_);
}

void SessionController::updatePolling(Qt::ApplicationState applicationState)
{
    if (signedIn_ && applicationState == Qt::ApplicationActive) {
        if (!activeRefreshTimer_.isActive()) {
            activeRefreshTimer_.start();
        }
    } else {
        activeRefreshTimer_.stop();
    }
}

bool SessionController::isCurrentResponse(const QString& op, quint64 requestId) const
{
    if (requestId == 0) {
        return true;
    }
    if (op == QLatin1String("profile")) {
        const auto generation = profileRequestGenerations_.constFind(requestId);
        const auto duringMutation = profileRequestsDuringMutation_.constFind(requestId);
        return requestId == latestProfileRequestId_
            && generation != profileRequestGenerations_.cend()
            && generation.value() == profileMutationGeneration_
            && duringMutation != profileRequestsDuringMutation_.cend()
            && !duringMutation.value();
    }
    if (op == QLatin1String("profile-update")) {
        return requestId == latestProfileUpdateRequestId_;
    }
    if (op == QLatin1String("rooms")) {
        const auto generation = roomRequestGenerations_.constFind(requestId);
        const auto duringMutation = roomRequestsDuringMutation_.constFind(requestId);
        return requestId == latestRoomsRequestId_
            && generation != roomRequestGenerations_.cend()
            && generation.value() == homeMutationGeneration_
            && duringMutation != roomRequestsDuringMutation_.cend()
            && !duringMutation.value();
    }
    if (op == QLatin1String("geocode") || op == QLatin1String("forecast")) {
        return requestId == latestWeatherRequestId_;
    }
    const auto entityKey = entityKeysByRequestId_.constFind(requestId);
    return entityKey == entityKeysByRequestId_.cend()
        || latestEntityRequestIds_.value(entityKey.value()) == requestId;
}

void SessionController::finishTrackedResponse(quint64 requestId)
{
    if (requestId == 0) {
        return;
    }
    roomRequestGenerations_.remove(requestId);
    roomRequestsDuringMutation_.remove(requestId);
    profileRequestGenerations_.remove(requestId);
    profileRequestsDuringMutation_.remove(requestId);
    pendingProfileMutations_.remove(requestId);
    pendingHomeMutations_.remove(requestId);
    entityKeysByRequestId_.remove(requestId);
}

void SessionController::clearPendingDeviceToggle(bool restore)
{
    if (pendingDeviceOnId_.isEmpty()) {
        return;
    }
    if (restore && previousDeviceOn_.has_value()) {
        for (RoomRow& room : roomRows_) {
            for (DeviceRow& device : room.devices) {
                if (device.id == pendingDeviceOnId_) {
                    device.isOn = previousDeviceOn_;
                }
            }
        }
        emit roomsChanged();
    }
    pendingDeviceOnId_.clear();
    previousDeviceOn_.reset();
}

void SessionController::setStatus(const QString& message)
{
    if (statusMessage_ == message) {
        return;
    }
    statusMessage_ = message;
    emit statusChanged();
}