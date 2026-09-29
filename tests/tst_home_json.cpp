#include "home_json.h"

#include <QTest>
#include <QTimeZone>

class HomeJsonTest : public QObject
{
    Q_OBJECT

private slots:
    void sessionFromUser();
    void sessionFromJwtSubject();
    void sessionWithoutTokens();
    void errorMessages();
    void profileAndRooms();
    void skipsInvalidDevice();
    void geocodingAndForecast();
    void weatherLabelAndIcon();
};

void HomeJsonTest::sessionFromUser()
{
    const QByteArray body = R"({
        "access_token": "header.payload.sig",
        "token_type": "bearer",
        "expires_in": 3600,
        "refresh_token": "refresh-1",
        "user": {"id": "123e4567-e89b-12d3-a456-426614174000", "email": "a@b.c"}
    })";
    const std::optional<SessionTokens> session = parseSession(body);
    QVERIFY(session.has_value());
    QCOMPARE(session->expiresIn, 3600);
    QCOMPARE(session->refreshToken, QStringLiteral("refresh-1"));
    QCOMPARE(session->userId, QStringLiteral("123e4567-e89b-12d3-a456-426614174000"));
    QCOMPARE(session->email, QStringLiteral("a@b.c"));
}

void HomeJsonTest::sessionFromJwtSubject()
{
    const QByteArray payload = QByteArrayLiteral(
        R"({"sub":"123e4567-e89b-12d3-a456-426614174000","exp":1790000000})");
    const QString token = QStringLiteral("aaa.")
        + QString::fromLatin1(payload.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals))
        + QStringLiteral(".bbb");
    const QByteArray body = QStringLiteral(R"({"access_token":"%1","refresh_token":"r"})").arg(token).toUtf8();
    const std::optional<SessionTokens> session = parseSession(body);
    QVERIFY(session.has_value());
    QCOMPARE(session->userId, QStringLiteral("123e4567-e89b-12d3-a456-426614174000"));
    QCOMPARE(jwtExpiryUtc(token), QDateTime::fromSecsSinceEpoch(1790000000, QTimeZone::utc()));
    QVERIFY(!jwtExpiryUtc(QStringLiteral("not-a-jwt")).isValid());
}

void HomeJsonTest::sessionWithoutTokens()
{
    QVERIFY(!parseSession(QByteArrayLiteral(R"({"id":"123e4567-e89b-12d3-a456-426614174000"})")).has_value());
    QVERIFY(!parseSession(QByteArrayLiteral("not-json")).has_value());
}

void HomeJsonTest::errorMessages()
{
    QCOMPARE(parseErrorMessage(QByteArrayLiteral(
                 R"({"error":"invalid_grant","error_description":"Invalid login credentials"})")),
        QStringLiteral("Invalid login credentials"));
    QCOMPARE(parseErrorMessage(QByteArrayLiteral(
                 R"({"code":"42501","message":"permission denied for table devices"})")),
        QStringLiteral("permission denied for table devices"));
    QCOMPARE(parseErrorMessage(QByteArrayLiteral(R"({"msg":"validation failed"})")),
        QStringLiteral("validation failed"));
    QVERIFY(parseErrorMessage(QByteArrayLiteral("nope")).isEmpty());
}

void HomeJsonTest::profileAndRooms()
{
    const std::optional<ProfileRow> profile = parseProfile(QByteArrayLiteral(
        R"([{"id":"123e4567-e89b-12d3-a456-426614174000","first_name":"Amina","city":null}])"));
    QVERIFY(profile.has_value());
    QCOMPARE(profile->firstName, QStringLiteral("Amina"));
    QVERIFY(profile->city.isEmpty());
    QVERIFY(!parseProfile(QByteArrayLiteral("[]")).has_value());

    const QByteArray rooms = R"([{
        "id":"123e4567-e89b-12d3-a456-426614174000",
        "name":"Kitchen",
        "position":1,
        "devices":[{
            "id":"123e4567-e89b-12d3-a456-426614174001",
            "name":"Probe",
            "kind":"thermometer",
            "is_on":null,
            "celsius":22.0,
            "reading_at":"2026-09-29T12:00:00.123456+00:00",
            "position":0
        }]
    }])";
    const std::optional<QList<RoomRow>> parsed = parseRooms(rooms);
    QVERIFY(parsed.has_value());
    QCOMPARE(parsed->size(), 1);
    QCOMPARE(parsed->at(0).devices.size(), 1);
    QCOMPARE(parsed->at(0).devices.at(0).roomId, parsed->at(0).id);
    QCOMPARE(*parsed->at(0).devices.at(0).celsius, 22.0);
    QCOMPARE(parsed->at(0).devices.at(0).readingAt,
        QDateTime::fromString(QStringLiteral("2026-09-29T12:00:00+00:00"), Qt::ISODate));
    const std::optional<QList<RoomRow>> empty = parseRooms(QByteArrayLiteral("[]"));
    QVERIFY(empty.has_value());
    QVERIFY(empty->isEmpty());
    QVERIFY(!parseRooms(QByteArrayLiteral("{}")).has_value());
}

void HomeJsonTest::skipsInvalidDevice()
{
    const QByteArray body = R"([{
        "id":"123e4567-e89b-12d3-a456-426614174010",
        "room_id":"123e4567-e89b-12d3-a456-426614174000",
        "name":"Lamp",
        "kind":"light",
        "is_on":true,
        "celsius":null,
        "reading_at":null,
        "position":2
    },{
        "id":"not-a-uuid",
        "name":"Bad",
        "kind":"plug",
        "is_on":false
    }])";
    const std::optional<QList<DeviceRow>> devices = parseDevices(body);
    QVERIFY(devices.has_value());
    QCOMPARE(devices->size(), 1);
    QVERIFY(devices->at(0).isOn.has_value());
    QCOMPARE(*devices->at(0).isOn, true);
}

void HomeJsonTest::geocodingAndForecast()
{
    const std::optional<GeoHit> hit = parseGeocoding(QByteArrayLiteral(
        R"({"results":[{"name":"Nairobi","latitude":-1.28,"longitude":36.82}]})"));
    QVERIFY(hit.has_value());
    QCOMPARE(hit->name, QStringLiteral("Nairobi"));
    QCOMPARE(hit->latitude, -1.28);
    QVERIFY(!parseGeocoding(QByteArrayLiteral(R"({"results":[]})")).has_value());
    QVERIFY(!parseGeocoding(QByteArrayLiteral(R"({"error":true,"reason":"not found"})")).has_value());

    const std::optional<ForecastNow> now = parseForecast(QByteArrayLiteral(
        R"({"current":{"temperature_2m":21.5,"weather_code":0,"is_day":1}})"));
    QVERIFY(now.has_value());
    QCOMPARE(now->temperatureCelsius, 21.5);
    QCOMPARE(now->weatherCode, 0);
    QVERIFY(now->isDay);
    QVERIFY(!parseForecast(QByteArrayLiteral("{}")).has_value());
}

void HomeJsonTest::weatherLabelAndIcon()
{
    QCOMPARE(weatherLabel(0), QStringLiteral("Clear sky"));
    QCOMPARE(weatherLabel(2), QStringLiteral("Partly cloudy"));
    QCOMPARE(weatherLabel(97), QStringLiteral("Heavy thunderstorm"));
    QCOMPARE(weatherLabel(4), QStringLiteral("Weather"));
    QCOMPARE(weatherIconFile(0, true), QStringLiteral("yellow-sun.svg"));
    QCOMPARE(weatherIconFile(0, false), QStringLiteral("white-moon.svg"));
    QCOMPARE(weatherIconFile(2, true), QStringLiteral("sun-cloud.svg"));
}

QTEST_GUILESS_MAIN(HomeJsonTest)
#include "tst_home_json.moc"
