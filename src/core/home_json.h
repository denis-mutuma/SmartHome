#pragma once

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

QString parseErrorMessage(const QByteArray& body);
