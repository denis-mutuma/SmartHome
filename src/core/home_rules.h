#pragma once

#include <QString>
#include <QTime>

QString greetingFor(QTime time);

bool isUuid(const QString& value);
bool isNameOk(const QString& trimmed);
bool isPasswordOk(const QString& password);
bool isEmailOk(const QString& email);
bool isCityOk(const QString& trimmedCity);
