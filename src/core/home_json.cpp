#include "home_json.h"

#include "home_rules.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStringList>
#include <QTimeZone>

#include <initializer_list>

namespace {

QJsonObject jwtPayload(const QString& accessToken)
{
    const QStringList parts = accessToken.split(QLatin1Char('.'));
    if (parts.size() != 3 || parts.at(1).isEmpty()) {
        return {};
    }
    QByteArray payload = parts.at(1).toLatin1();
    while (payload.size() % 4 != 0) {
        payload.append('=');
    }
    const QJsonDocument document = QJsonDocument::fromJson(
        QByteArray::fromBase64(payload, QByteArray::Base64UrlEncoding));
    return document.isObject() ? document.object() : QJsonObject();
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

bool switchableKind(const QString& kind)
{
    return kind == QLatin1String("light") || kind == QLatin1String("plug")
        || kind == QLatin1String("thermometer");
}

std::optional<DeviceRow> deviceFrom(const QJsonValue& value, const QString& parentRoomId)
{
    if (!value.isObject()) {
        return std::nullopt;
    }
    const QJsonObject object = value.toObject();
    const QJsonValue id = object.value(QLatin1String("id"));
    const QJsonValue roomIdValue = object.value(QLatin1String("room_id"));
    const QJsonValue name = object.value(QLatin1String("name"));
    const QJsonValue kind = object.value(QLatin1String("kind"));
    const QJsonValue isOn = object.value(QLatin1String("is_on"));
    const QJsonValue celsius = object.value(QLatin1String("celsius"));
    const QJsonValue readingAt = object.value(QLatin1String("reading_at"));
    QString roomId;
    if (roomIdValue.isString() && !roomIdValue.toString().isEmpty()) {
        roomId = roomIdValue.toString();
    } else if (roomIdValue.isNull() || roomIdValue.isUndefined()
        || (roomIdValue.isString() && roomIdValue.toString().isEmpty())) {
        roomId = parentRoomId;
    } else {
        return std::nullopt;
    }
    const bool thermometer = kind.isString() && kind.toString() == QLatin1String("thermometer");
    if (!id.isString() || !isUuid(id.toString()) || !isUuid(roomId)
        || !name.isString() || name.toString().isEmpty() || !kind.isString() || !switchableKind(kind.toString())
        || (thermometer && ((!isOn.isNull() && !isOn.isUndefined()) || !celsius.isDouble() || !readingAt.isString()))
        || (!thermometer && (!isOn.isBool() || (!celsius.isNull() && !celsius.isUndefined())
            || (!readingAt.isNull() && !readingAt.isUndefined())))) {
        return std::nullopt;
    }
    if (!parentRoomId.isEmpty() && roomId != parentRoomId) {
        return std::nullopt;
    }

    DeviceRow row;
    row.id = id.toString();
    row.roomId = roomId;
    row.name = name.toString();
    row.kind = kind.toString();
    row.position = object.value(QLatin1String("position")).toInt();
    if (thermometer) {
        row.celsius = celsius.toDouble();
        row.readingAt = timestampFrom(readingAt);
        if (!row.readingAt.isValid()) {
            return std::nullopt;
        }
    } else {
        row.isOn = isOn.toBool();
    }
    return row;
}

QList<DeviceRow> devicesFrom(const QJsonArray& array, const QString& parentRoomId)
{
    QList<DeviceRow> devices;
    devices.reserve(array.size());
    for (const QJsonValue& value : array) {
        if (const std::optional<DeviceRow> device = deviceFrom(value, parentRoomId)) {
            devices.append(*device);
        }
    }
    return devices;
}

} // namespace

std::optional<SessionTokens> parseSession(const QByteArray& body)
{
    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (!document.isObject()) {
        return std::nullopt;
    }
    const QJsonObject object = document.object();
    const QJsonValue accessToken = object.value(QLatin1String("access_token"));
    const QJsonValue refreshToken = object.value(QLatin1String("refresh_token"));
    const QJsonValue userValue = object.value(QLatin1String("user"));
    if (!accessToken.isString() || accessToken.toString().isEmpty()
        || !refreshToken.isString() || refreshToken.toString().isEmpty() || !userValue.isObject()) {
        return std::nullopt;
    }

    const QJsonObject user = userValue.toObject();
    const QJsonValue userId = user.value(QLatin1String("id"));
    SessionTokens session;
    session.accessToken = accessToken.toString();
    session.refreshToken = refreshToken.toString();
    session.expiresIn = object.value(QLatin1String("expires_in")).toInt();
    session.userId = userId.isString() ? userId.toString() : QString();
    if (!isUuid(session.userId)) {
        session.userId = jwtPayload(session.accessToken).value(QLatin1String("sub")).toString();
    }
    if (!isUuid(session.userId)) {
        return std::nullopt;
    }
    session.email = user.value(QLatin1String("email")).toString();
    return session;
}

QDateTime jwtExpiryUtc(const QString& accessToken)
{
    const QJsonValue expiry = jwtPayload(accessToken).value(QLatin1String("exp"));
    if (!expiry.isDouble()) {
        return {};
    }
    return QDateTime::fromSecsSinceEpoch(static_cast<qint64>(expiry.toDouble()), QTimeZone::utc());
}

QString parseErrorMessage(const QByteArray& body)
{
    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (!document.isObject()) {
        return {};
    }
    const QJsonObject object = document.object();
    for (const char* key : {"error_description", "message", "msg", "error"}) {
        const QJsonValue value = object.value(QLatin1String(key));
        if (value.isString() && !value.toString().isEmpty()) {
            return value.toString();
        }
    }
    return {};
}

std::optional<ProfileRow> parseProfile(const QByteArray& body)
{
    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (!document.isArray() || document.array().isEmpty() || !document.array().at(0).isObject()) {
        return std::nullopt;
    }

    const QJsonObject object = document.array().at(0).toObject();
    const QJsonValue id = object.value(QLatin1String("id"));
    const QJsonValue firstName = object.value(QLatin1String("first_name"));
    const QJsonValue city = object.value(QLatin1String("city"));
    if (!id.isString() || !isUuid(id.toString()) || !firstName.isString() || firstName.toString().isEmpty()) {
        return std::nullopt;
    }
    if (!city.isString() && !city.isNull() && !city.isUndefined()) {
        return std::nullopt;
    }

    ProfileRow row;
    row.id = id.toString();
    row.firstName = firstName.toString();
    row.city = city.toString();
    return row;
}

std::optional<QList<DeviceRow>> parseDevices(const QByteArray& body)
{
    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (!document.isArray()) {
        return std::nullopt;
    }

    return devicesFrom(document.array(), QString());
}

std::optional<QList<RoomRow>> parseRooms(const QByteArray& body)
{
    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (!document.isArray()) {
        return std::nullopt;
    }

    QList<RoomRow> rooms;
    for (const QJsonValue& value : document.array()) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject object = value.toObject();
        const QJsonValue id = object.value(QLatin1String("id"));
        const QJsonValue name = object.value(QLatin1String("name"));
        if (!id.isString() || !isUuid(id.toString()) || !name.isString() || name.toString().isEmpty()) {
            continue;
        }

        RoomRow room;
        room.id = id.toString();
        room.name = name.toString();
        room.position = object.value(QLatin1String("position")).toInt();
        const QJsonValue devices = object.value(QLatin1String("devices"));
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
    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (!document.isObject()) {
        return std::nullopt;
    }
    const QJsonValue results = document.object().value(QLatin1String("results"));
    if (!results.isArray() || results.toArray().isEmpty() || !results.toArray().at(0).isObject()) {
        return std::nullopt;
    }

    const QJsonObject first = results.toArray().at(0).toObject();
    const QJsonValue name = first.value(QLatin1String("name"));
    const QJsonValue latitude = first.value(QLatin1String("latitude"));
    const QJsonValue longitude = first.value(QLatin1String("longitude"));
    if (!name.isString() || !latitude.isDouble() || !longitude.isDouble()) {
        return std::nullopt;
    }

    GeoHit hit;
    hit.name = name.toString();
    hit.latitude = latitude.toDouble();
    hit.longitude = longitude.toDouble();
    return hit;
}
