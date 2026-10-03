#include "home_json.h"

#include "home_rules.h"

#include <QJsonDocument>
#include <QJsonObject>

#include <initializer_list>

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
    if (!userId.isString() || !isUuid(userId.toString())) {
        return std::nullopt;
    }

    SessionTokens session;
    session.accessToken = accessToken.toString();
    session.refreshToken = refreshToken.toString();
    session.expiresIn = object.value(QLatin1String("expires_in")).toInt();
    session.userId = userId.toString();
    session.email = user.value(QLatin1String("email")).toString();
    return session;
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