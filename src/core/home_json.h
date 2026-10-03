#pragma once

#include <QDateTime>
#include <QList>
#include <QByteArray>
#include <QString>

#include <optional>

struct SessionTokens {
    QString accessToken;
    QString refreshToken;
    int expiresIn = 0;
    QString userId;
    QString email;
};

struct ProfileRow {
    QString id;
    QString firstName;
    QString city;
};

struct DeviceRow {
    QString id;
    QString roomId;
    QString name;
    QString kind;
    std::optional<bool> isOn;
    std::optional<double> celsius;
    QDateTime readingAt;
    int position = 0;
};

struct RoomRow {
    QString id;
    QString name;
    int position = 0;
    QList<DeviceRow> devices;
};

struct GeoHit {
    QString name;
    double latitude = 0.0;
    double longitude = 0.0;
};

struct ForecastNow {
    double temperatureCelsius = 0.0;
    int weatherCode = 0;
    bool isDay = false;
};

std::optional<SessionTokens> parseSession(const QByteArray& body);
QDateTime jwtExpiryUtc(const QString& accessToken);

QString parseErrorMessage(const QByteArray& body);
std::optional<ProfileRow> parseProfile(const QByteArray& body);
std::optional<QList<RoomRow>> parseRooms(const QByteArray& body);
std::optional<QList<DeviceRow>> parseDevices(const QByteArray& body);
std::optional<GeoHit> parseGeocoding(const QByteArray& body);
std::optional<ForecastNow> parseForecast(const QByteArray& body);
