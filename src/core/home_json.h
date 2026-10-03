#pragma once

#include <QDateTime>
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

std::optional<SessionTokens> parseSession(const QByteArray& body);
QDateTime jwtExpiryUtc(const QString& accessToken);

QString parseErrorMessage(const QByteArray& body);
std::optional<ProfileRow> parseProfile(const QByteArray& body);
