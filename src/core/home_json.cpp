#include "home_json.h"

#include "home_rules.h"

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