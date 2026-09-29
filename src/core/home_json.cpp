#include "home_json.h"

#include "home_rules.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTimeZone>

namespace {

QJsonObject objectFrom(const QByteArray& body)
{
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    return doc.isObject() ? doc.object() : QJsonObject();
}

QJsonObject jwtPayload(const QString& accessToken)
{
    const QStringList parts = accessToken.split(QLatin1Char('.'));
    if (parts.size() < 2) {
        return {};
    }
    QByteArray payload = parts.at(1).toLatin1();
    while (payload.size() % 4 != 0) {
        payload.append('=');
    }
    const QJsonDocument doc = QJsonDocument::fromJson(
        QByteArray::fromBase64(payload, QByteArray::Base64UrlEncoding));
    return doc.isObject() ? doc.object() : QJsonObject();
}

QString stringField(const QJsonObject& obj, const char* key)
{
    const QJsonValue value = obj.value(QLatin1String(key));
    return value.isString() ? value.toString() : QString();
}

QDateTime timestampFrom(const QJsonValue& value)
{
    if (!value.isString()) {
        return {};
    }
    static const QRegularExpression fraction(QStringLiteral("\\.\\d+"));
    QString text = value.toString();
    text.replace(fraction, QString());
    return QDateTime::fromString(text, Qt::ISODate);
}

bool knownKind(const QString& kind)
{
    return kind == QLatin1String("light") || kind == QLatin1String("plug")
        || kind == QLatin1String("thermometer");
}

std::optional<DeviceRow> deviceFrom(const QJsonValue& value, const QString& parentRoomId)
{
    if (!value.isObject()) {
        return std::nullopt;
    }
    const QJsonObject obj = value.toObject();
    DeviceRow row;
    row.id = stringField(obj, "id");
    row.roomId = stringField(obj, "room_id");
    if (row.roomId.isEmpty()) {
        row.roomId = parentRoomId;
    }
    row.name = stringField(obj, "name");
    row.kind = stringField(obj, "kind");
    row.position = obj.value(QLatin1String("position")).toInt();
    if (!isUuid(row.id) || !isUuid(row.roomId) || !knownKind(row.kind) || row.name.isEmpty()) {
        return std::nullopt;
    }
    if (!parentRoomId.isEmpty() && row.roomId != parentRoomId) {
        return std::nullopt;
    }

    const QJsonValue on = obj.value(QLatin1String("is_on"));
    if (on.isBool()) {
        row.isOn = on.toBool();
    } else if (!on.isNull() && !on.isUndefined()) {
        return std::nullopt;
    }
    const QJsonValue celsius = obj.value(QLatin1String("celsius"));
    if (celsius.isDouble()) {
        row.celsius = celsius.toDouble();
    } else if (!celsius.isNull() && !celsius.isUndefined()) {
        return std::nullopt;
    }
    const QJsonValue reading = obj.value(QLatin1String("reading_at"));
    if (reading.isString()) {
        row.readingAt = timestampFrom(reading);
        if (!row.readingAt.isValid()) {
            return std::nullopt;
        }
    } else if (!reading.isNull() && !reading.isUndefined()) {
        return std::nullopt;
    }

    const bool thermometer = row.kind == QLatin1String("thermometer");
    if (thermometer) {
        if (row.isOn.has_value() || !row.celsius.has_value() || !row.readingAt.isValid()) {
            return std::nullopt;
        }
    } else if (!row.isOn.has_value() || row.celsius.has_value() || row.readingAt.isValid()) {
        return std::nullopt;
    }
    return row;
}

QList<DeviceRow> devicesFrom(const QJsonArray& array, const QString& parentRoomId)
{
    QList<DeviceRow> rows;
    rows.reserve(array.size());
    for (const QJsonValue& value : array) {
        if (const std::optional<DeviceRow> row = deviceFrom(value, parentRoomId)) {
            rows.append(*row);
        }
    }
    return rows;
}

} // namespace

std::optional<SessionTokens> parseSession(const QByteArray& body)
{
    const QJsonObject obj = objectFrom(body);
    if (obj.isEmpty()) {
        return std::nullopt;
    }
    SessionTokens session;
    session.accessToken = stringField(obj, "access_token");
    session.refreshToken = stringField(obj, "refresh_token");
    const QJsonValue user = obj.value(QLatin1String("user"));
    if (user.isObject()) {
        session.userId = stringField(user.toObject(), "id");
        session.email = stringField(user.toObject(), "email");
    }
    if (!isUuid(session.userId)) {
        session.userId = stringField(jwtPayload(session.accessToken), "sub");
    }
    if (session.accessToken.isEmpty() || session.refreshToken.isEmpty() || !isUuid(session.userId)) {
        return std::nullopt;
    }
    return session;
}

QDateTime jwtExpiryUtc(const QString& accessToken)
{
    const QJsonValue exp = jwtPayload(accessToken).value(QLatin1String("exp"));
    if (!exp.isDouble()) {
        return {};
    }
    return QDateTime::fromSecsSinceEpoch(static_cast<qint64>(exp.toDouble()), QTimeZone::utc());
}

QString parseErrorMessage(const QByteArray& body)
{
    const QJsonObject obj = objectFrom(body);
    const QString description = stringField(obj, "error_description");
    if (!description.isEmpty()) {
        return description;
    }
    const QString message = stringField(obj, "message");
    if (!message.isEmpty()) {
        return message;
    }
    const QString msg = stringField(obj, "msg");
    if (!msg.isEmpty()) {
        return msg;
    }
    return stringField(obj, "error");
}

std::optional<ProfileRow> parseProfile(const QByteArray& body)
{
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isArray() || doc.array().isEmpty()) {
        return std::nullopt;
    }
    const QJsonValue first = doc.array().at(0);
    if (!first.isObject()) {
        return std::nullopt;
    }
    const QJsonObject obj = first.toObject();
    ProfileRow row;
    row.id = stringField(obj, "id");
    row.firstName = stringField(obj, "first_name");
    row.city = stringField(obj, "city");
    if (!isUuid(row.id) || row.firstName.isEmpty()) {
        return std::nullopt;
    }
    return row;
}

std::optional<QList<DeviceRow>> parseDevices(const QByteArray& body)
{
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isArray()) {
        return std::nullopt;
    }
    return devicesFrom(doc.array(), QString());
}

std::optional<QList<RoomRow>> parseRooms(const QByteArray& body)
{
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isArray()) {
        return std::nullopt;
    }
    QList<RoomRow> rooms;
    for (const QJsonValue& value : doc.array()) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject obj = value.toObject();
        RoomRow room;
        room.id = stringField(obj, "id");
        room.name = stringField(obj, "name");
        room.position = obj.value(QLatin1String("position")).toInt();
        if (!isUuid(room.id) || room.name.isEmpty()) {
            continue;
        }
        const QJsonValue devices = obj.value(QLatin1String("devices"));
        if (devices.isArray()) {
            room.devices = devicesFrom(devices.toArray(), room.id);
        } else if (!devices.isUndefined() && !devices.isNull()) {
            continue;
        }
        rooms.append(room);
    }
    return rooms;
}

std::optional<GeoHit> parseGeocoding(const QByteArray& body)
{
    const QJsonValue results = objectFrom(body).value(QLatin1String("results"));
    if (!results.isArray() || results.toArray().isEmpty() || !results.toArray().at(0).isObject()) {
        return std::nullopt;
    }
    const QJsonObject first = results.toArray().at(0).toObject();
    const QJsonValue latitude = first.value(QLatin1String("latitude"));
    const QJsonValue longitude = first.value(QLatin1String("longitude"));
    if (!latitude.isDouble() || !longitude.isDouble() || !first.value(QLatin1String("name")).isString()) {
        return std::nullopt;
    }
    GeoHit hit;
    hit.name = first.value(QLatin1String("name")).toString();
    hit.latitude = latitude.toDouble();
    hit.longitude = longitude.toDouble();
    return hit;
}

std::optional<ForecastNow> parseForecast(const QByteArray& body)
{
    const QJsonValue current = objectFrom(body).value(QLatin1String("current"));
    if (!current.isObject()) {
        return std::nullopt;
    }
    const QJsonObject obj = current.toObject();
    const QJsonValue temperature = obj.value(QLatin1String("temperature_2m"));
    const QJsonValue code = obj.value(QLatin1String("weather_code"));
    const QJsonValue day = obj.value(QLatin1String("is_day"));
    if (!temperature.isDouble() || !code.isDouble() || !day.isDouble()) {
        return std::nullopt;
    }
    ForecastNow now;
    now.temperatureCelsius = temperature.toDouble();
    now.weatherCode = code.toInt();
    now.isDay = day.toInt() == 1;
    return now;
}

QString weatherLabel(int weatherCode)
{
    switch (weatherCode) {
    case 0: return QStringLiteral("Clear sky");
    case 1: return QStringLiteral("Mainly clear");
    case 2: return QStringLiteral("Partly cloudy");
    case 3: return QStringLiteral("Overcast");
    case 45: return QStringLiteral("Fog");
    case 48: return QStringLiteral("Depositing rime fog");
    case 51: return QStringLiteral("Light drizzle");
    case 53: return QStringLiteral("Moderate drizzle");
    case 55: return QStringLiteral("Dense drizzle");
    case 56: return QStringLiteral("Light freezing drizzle");
    case 57: return QStringLiteral("Dense freezing drizzle");
    case 61: return QStringLiteral("Slight rain");
    case 63: return QStringLiteral("Moderate rain");
    case 65: return QStringLiteral("Heavy rain");
    case 66: return QStringLiteral("Light freezing rain");
    case 67: return QStringLiteral("Heavy freezing rain");
    case 71: return QStringLiteral("Slight snowfall");
    case 73: return QStringLiteral("Moderate snowfall");
    case 75: return QStringLiteral("Heavy snowfall");
    case 77: return QStringLiteral("Snow grains");
    case 80: return QStringLiteral("Slight rain showers");
    case 81: return QStringLiteral("Moderate rain showers");
    case 82: return QStringLiteral("Violent rain showers");
    case 85: return QStringLiteral("Slight snow showers");
    case 86: return QStringLiteral("Heavy snow showers");
    case 95: return QStringLiteral("Thunderstorm");
    case 96: return QStringLiteral("Thunderstorm with slight hail");
    case 97: return QStringLiteral("Heavy thunderstorm");
    case 99: return QStringLiteral("Thunderstorm with heavy hail");
    default: return QStringLiteral("Weather");
    }
}

QString weatherIconFile(int weatherCode, bool isDay)
{
    if (weatherCode == 0) {
        return isDay ? QStringLiteral("yellow-sun.svg") : QStringLiteral("white-moon.svg");
    }
    return QStringLiteral("sun-cloud.svg");
}
