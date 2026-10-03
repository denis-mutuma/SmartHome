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

std::optional<SessionTokens> parseSession(const QByteArray& body);
QDateTime jwtExpiryUtc(const QString& accessToken);

QString parseErrorMessage(const QByteArray& body);
std::optional<ProfileRow> parseProfile(const QByteArray& body);
std::optional<QList<DeviceRow>> parseDevices(const QByteArray& body);
