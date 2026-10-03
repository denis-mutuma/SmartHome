#include "home_json.h"

#include "home_rules.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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

bool switchableKind(const QString& kind)
{
    return kind == QLatin1String("light") || kind == QLatin1String("plug");
}

std::optional<DeviceRow> deviceFrom(const QJsonValue& value)
{
    if (!value.isObject()) {
        return std::nullopt;
    }
    const QJsonObject object = value.toObject();
    const QJsonValue id = object.value(QLatin1String("id"));
    const QJsonValue roomId = object.value(QLatin1String("room_id"));
    const QJsonValue name = object.value(QLatin1String("name"));
    const QJsonValue kind = object.value(QLatin1String("kind"));
    const QJsonValue isOn = object.value(QLatin1String("is_on"));
    const QJsonValue celsius = object.value(QLatin1String("celsius"));
    const QJsonValue readingAt = object.value(QLatin1String("reading_at"));
    if (!id.isString() || !isUuid(id.toString()) || !roomId.isString() || !isUuid(roomId.toString())
        || !name.isString() || name.toString().isEmpty() || !kind.isString() || !switchableKind(kind.toString())
        || !isOn.isBool() || (!celsius.isNull() && !celsius.isUndefined())
        || (!readingAt.isNull() && !readingAt.isUndefined())) {
        return std::nullopt;
    }

    DeviceRow row;
    row.id = id.toString();
    row.roomId = roomId.toString();
    row.name = name.toString();
    row.kind = kind.toString();
    row.isOn = isOn.toBool();
    row.position = object.value(QLatin1String("position")).toInt();
    return row;
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

    QList<DeviceRow> devices;
    for (const QJsonValue& value : document.array()) {
        if (const std::optional<DeviceRow> device = deviceFrom(value)) {
            devices.append(*device);
        }
    }
    return devices;
}
