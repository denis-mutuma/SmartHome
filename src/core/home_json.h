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

std::optional<SessionTokens> parseSession(const QByteArray& body);
QDateTime jwtExpiryUtc(const QString& accessToken);

QString parseErrorMessage(const QByteArray& body);
