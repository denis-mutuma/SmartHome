#pragma once

#include <QDateTime>
#include <QString>

QString greetingFor(QTime time);

double clampCelsius(double value);
double nextCelsius(double current, double delta);
bool readingIsStale(const QDateTime& readingAt, const QDateTime& now);

bool isUuid(const QString& value);
bool isNameOk(const QString& trimmed);
bool isPasswordOk(const QString& password);
bool isEmailOk(const QString& email);
bool isCityOk(const QString& trimmedCity);
