#include "api_client.h"

#include "home_json.h"
#include "home_rules.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

namespace {

constexpr int kTransferTimeoutMs = 15000;

QByteArray objectJson(const QJsonObject& object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

} // namespace

ApiClient::ApiClient(QString baseUrl, QString anonKey, QObject* parent)
    : QObject(parent)
    , network_(new QNetworkAccessManager(this))
    , baseUrl_(std::move(baseUrl))
    , anonKey_(std::move(anonKey))
{
    while (baseUrl_.endsWith(QLatin1Char('/'))) {
        baseUrl_.chop(1);
    }
}

void ApiClient::setAccessToken(const QString& token)
{
    accessToken_ = token;
}

bool ApiClient::guardConfig(const QString& op)
{
    if (!baseUrl_.isEmpty() && !anonKey_.isEmpty()) {
        return true;
    }
    emit failed(op, 0, QStringLiteral("Set the Supabase URL and anon key in config.local.cmake."));
    return false;
}

bool ApiClient::guardUuid(const QString& op, const QString& id)
{
    if (isUuid(id)) {
        return true;
    }
    emit failed(op, 0, QStringLiteral("The service could not complete the request."));
    return false;
}

QNetworkRequest ApiClient::makeRequest(const QUrl& url, bool authorize, bool represent, bool includeKey) const
{
    QNetworkRequest request(url);
    request.setTransferTimeout(kTransferTimeoutMs);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (includeKey && !anonKey_.isEmpty()) {
        request.setRawHeader("apikey", anonKey_.toUtf8());
    }
    if (authorize && !accessToken_.isEmpty()) {
        request.setRawHeader("Authorization", QByteArray("Bearer ") + accessToken_.toUtf8());
    }
    if (represent) {
        request.setRawHeader("Prefer", "return=representation");
    }
    return request;
}

void ApiClient::send(const QString& op, const QByteArray& method, const QUrl& url, const QByteArray& body, bool authorize, bool represent, bool includeKey)
{
    QNetworkRequest request = makeRequest(url, authorize, represent, includeKey);
    QNetworkReply* reply = network_->sendCustomRequest(request, method, body);
    reply->setProperty("op", op);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QString operation = reply->property("op").toString();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray payload = reply->readAll();
        const QNetworkReply::NetworkError error = reply->error();
        const QString errorString = reply->errorString();
        reply->deleteLater();
        if (error != QNetworkReply::NoError && status == 0) {
            emit failed(operation, status, errorString);
            return;
        }
        if (status >= 400) {
            const QString parsed = parseErrorMessage(payload);
            emit failed(operation, status,
                parsed.isEmpty() ? QStringLiteral("The service could not complete the request.") : parsed);
            return;
        }
        emit completed(operation, status, payload);
    });
}

void ApiClient::signUp(const QString& email, const QString& password, const QString& firstName)
{
    if (!guardConfig(QStringLiteral("signup"))) {
        return;
    }
    const QJsonObject body{{QStringLiteral("email"), email},
        {QStringLiteral("password"), password},
        {QStringLiteral("data"), QJsonObject{{QStringLiteral("first_name"), firstName}}}};
    send(QStringLiteral("signup"), "POST", QUrl(baseUrl_ + QStringLiteral("/auth/v1/signup")), objectJson(body), false, false, true);
}

void ApiClient::signIn(const QString& email, const QString& password)
{
    if (!guardConfig(QStringLiteral("login"))) {
        return;
    }
    const QJsonObject body{{QStringLiteral("email"), email}, {QStringLiteral("password"), password}};
    send(QStringLiteral("login"), "POST",
        QUrl(baseUrl_ + QStringLiteral("/auth/v1/token?grant_type=password")), objectJson(body), false, false, true);
}

void ApiClient::refresh(const QString& refreshToken)
{
    if (!guardConfig(QStringLiteral("refresh"))) {
        return;
    }
    const QJsonObject body{{QStringLiteral("refresh_token"), refreshToken}};
    send(QStringLiteral("refresh"), "POST",
        QUrl(baseUrl_ + QStringLiteral("/auth/v1/token?grant_type=refresh_token")), objectJson(body), false, false, true);
}

void ApiClient::logOut()
{
    if (!guardConfig(QStringLiteral("logout"))) {
        return;
    }
    send(QStringLiteral("logout"), "POST", QUrl(baseUrl_ + QStringLiteral("/auth/v1/logout")), QByteArrayLiteral("{}"), true, false, true);
}

void ApiClient::fetchProfile()
{
    if (!guardConfig(QStringLiteral("profile"))) {
        return;
    }
    send(QStringLiteral("profile"), "GET",
        QUrl(baseUrl_ + QStringLiteral("/rest/v1/profiles?select=id,first_name,city")), QByteArray(), true, false, true);
}

void ApiClient::updateProfile(const QString& userId, const QString& firstName, const QString& city)
{
    if (!guardConfig(QStringLiteral("profile-update")) || !guardUuid(QStringLiteral("profile-update"), userId)) {
        return;
    }
    const QJsonObject body{{QStringLiteral("first_name"), firstName}, {QStringLiteral("city"), city}};
    send(QStringLiteral("profile-update"), "PATCH",
        QUrl(baseUrl_ + QStringLiteral("/rest/v1/profiles?id=eq.") + userId + QStringLiteral("&select=id,first_name,city")),
        objectJson(body), true, true, true);
}

void ApiClient::fetchRooms()
{
    if (!guardConfig(QStringLiteral("rooms"))) {
        return;
    }
    send(QStringLiteral("rooms"), "GET",
        QUrl(baseUrl_
            + QStringLiteral("/rest/v1/rooms?select=id,name,position,"
                             "devices(id,room_id,name,kind,is_on,celsius,reading_at,position)"
                             "&order=position.asc&devices.order=position.asc")),
        QByteArray(), true, false, true);
}

void ApiClient::insertRoom(const QString& name, int position)
{
    if (!guardConfig(QStringLiteral("room-insert"))) {
        return;
    }
    const QJsonObject body{{QStringLiteral("name"), name}, {QStringLiteral("position"), position}};
    send(QStringLiteral("room-insert"), "POST",
        QUrl(baseUrl_ + QStringLiteral("/rest/v1/rooms?select=id,name,position")), objectJson(body), true, true, true);
}

void ApiClient::updateRoom(const QString& id, const QString& name)
{
    if (!guardConfig(QStringLiteral("room-update")) || !guardUuid(QStringLiteral("room-update"), id)) {
        return;
    }
    const QJsonObject body{{QStringLiteral("name"), name}};
    send(QStringLiteral("room-update"), "PATCH",
        QUrl(baseUrl_ + QStringLiteral("/rest/v1/rooms?id=eq.") + id + QStringLiteral("&select=id,name,position")),
        objectJson(body), true, true, true);
}

void ApiClient::deleteRoom(const QString& id)
{
    if (!guardConfig(QStringLiteral("room-delete")) || !guardUuid(QStringLiteral("room-delete"), id)) {
        return;
    }
    send(QStringLiteral("room-delete"), "DELETE",
        QUrl(baseUrl_ + QStringLiteral("/rest/v1/rooms?id=eq.") + id), QByteArray(), true, false, true);
}

void ApiClient::insertDevice(const QString& roomId, const QString& name, const QString& kind, int position)
{
    if (!guardConfig(QStringLiteral("device-insert")) || !guardUuid(QStringLiteral("device-insert"), roomId)) {
        return;
    }
    QJsonObject body{{QStringLiteral("room_id"), roomId},
        {QStringLiteral("name"), name},
        {QStringLiteral("kind"), kind},
        {QStringLiteral("position"), position}};
    if (kind == QLatin1String("thermometer")) {
        body.insert(QStringLiteral("celsius"), 22.0);
        body.insert(QStringLiteral("reading_at"),
            QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    } else {
        body.insert(QStringLiteral("is_on"), false);
    }
    send(QStringLiteral("device-insert"), "POST",
        QUrl(baseUrl_
            + QStringLiteral("/rest/v1/devices?select=id,room_id,name,kind,is_on,celsius,reading_at,position")),
        objectJson(body), true, true, true);
}

void ApiClient::setDeviceOn(const QString& id, bool on)
{
    if (!guardConfig(QStringLiteral("device-on")) || !guardUuid(QStringLiteral("device-on"), id)) {
        return;
    }
    const QJsonObject body{{QStringLiteral("is_on"), on}};
    send(QStringLiteral("device-on"), "PATCH",
        QUrl(baseUrl_ + QStringLiteral("/rest/v1/devices?id=eq.") + id
            + QStringLiteral("&select=id,room_id,name,kind,is_on,celsius,reading_at,position")),
        objectJson(body), true, true, true);
}

void ApiClient::setReading(const QString& id, double celsius, const QDateTime& readingAt)
{
    if (!guardConfig(QStringLiteral("device-reading")) || !guardUuid(QStringLiteral("device-reading"), id)) {
        return;
    }
    const QJsonObject body{{QStringLiteral("celsius"), celsius},
        {QStringLiteral("reading_at"), readingAt.toUTC().toString(Qt::ISODateWithMs)}};
    send(QStringLiteral("device-reading"), "PATCH",
        QUrl(baseUrl_ + QStringLiteral("/rest/v1/devices?id=eq.") + id
            + QStringLiteral("&select=id,room_id,name,kind,is_on,celsius,reading_at,position")),
        objectJson(body), true, true, true);
}

void ApiClient::updateDeviceName(const QString& id, const QString& name)
{
    if (!guardConfig(QStringLiteral("device-update")) || !guardUuid(QStringLiteral("device-update"), id)) {
        return;
    }
    const QJsonObject body{{QStringLiteral("name"), name}};
    send(QStringLiteral("device-update"), "PATCH",
        QUrl(baseUrl_ + QStringLiteral("/rest/v1/devices?id=eq.") + id
            + QStringLiteral("&select=id,room_id,name,kind,is_on,celsius,reading_at,position")),
        objectJson(body), true, true, true);
}

void ApiClient::deleteDevice(const QString& id)
{
    if (!guardConfig(QStringLiteral("device-delete")) || !guardUuid(QStringLiteral("device-delete"), id)) {
        return;
    }
    send(QStringLiteral("device-delete"), "DELETE",
        QUrl(baseUrl_ + QStringLiteral("/rest/v1/devices?id=eq.") + id), QByteArray(), true, false, true);
}

void ApiClient::geocode(const QString& city)
{
    QUrl url(QStringLiteral("https://geocoding-api.open-meteo.com/v1/search"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("name"), city);
    query.addQueryItem(QStringLiteral("count"), QStringLiteral("1"));
    query.addQueryItem(QStringLiteral("language"), QStringLiteral("en"));
    query.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    url.setQuery(query);
    send(QStringLiteral("geocode"), "GET", url, QByteArray(), false, false, false);
}

void ApiClient::forecast(double latitude, double longitude)
{
    QUrl url(QStringLiteral("https://api.open-meteo.com/v1/forecast"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("latitude"), QString::number(latitude, 'f', 6));
    query.addQueryItem(QStringLiteral("longitude"), QString::number(longitude, 'f', 6));
    query.addQueryItem(QStringLiteral("current"), QStringLiteral("temperature_2m,weather_code,is_day"));
    url.setQuery(query);
    send(QStringLiteral("forecast"), "GET", url, QByteArray(), false, false, false);
}
