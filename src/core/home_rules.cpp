#include "home_rules.h"

#include <QRegularExpression>

#include <algorithm>
#include <cmath>

QString greetingFor(QTime time)
{
    const int minutes = time.hour() * 60 + time.minute();
    if (minutes >= 5 * 60 && minutes <= 11 * 60 + 59) {
        return QStringLiteral("Good morning,");
    }
    if (minutes >= 12 * 60 && minutes <= 16 * 60 + 59) {
        return QStringLiteral("Good afternoon,");
    }
    if (minutes >= 17 * 60 && minutes <= 20 * 60 + 59) {
        return QStringLiteral("Good evening,");
    }
    return QStringLiteral("Good night,");
}

double clampCelsius(double value)
{
    const double clamped = std::clamp(value, 18.0, 28.0);
    return std::round(clamped * 10.0) / 10.0;
}

double nextCelsius(double current, double delta)
{
    return clampCelsius(current + delta);
}

bool readingIsStale(const QDateTime& readingAt, const QDateTime& now)
{
    if (!readingAt.isValid() || !now.isValid()) {
        return true;
    }
    return readingAt.secsTo(now) >= 15 * 60;
}

bool isUuid(const QString& value)
{
    static const QRegularExpression pattern(
        QStringLiteral("^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-"
                       "[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$"));
    return pattern.match(value).hasMatch();
}

bool isNameOk(const QString& trimmed)
{
    const int length = trimmed.size();
    return length >= 1 && length <= 40;
}

bool isPasswordOk(const QString& password)
{
    const int length = password.size();
    return length >= 8 && length <= 72;
}

bool isEmailOk(const QString& email)
{
    const QString trimmed = email.trimmed();
    if (trimmed.size() < 3 || trimmed.size() > 254 || trimmed.contains(QLatin1Char(' '))) {
        return false;
    }
    const int at = trimmed.indexOf(QLatin1Char('@'));
    return at > 0 && at < trimmed.size() - 1 && trimmed.indexOf(QLatin1Char('@'), at + 1) < 0;
}

bool isCityOk(const QString& trimmedCity)
{
    const int length = trimmedCity.size();
    return length == 0 || (length >= 1 && length <= 80);
}
