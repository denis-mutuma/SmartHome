#include "home_json.h"

#include <QTest>
#include <QTimeZone>

namespace {

QString tokenForPayload(const QByteArray& payload, bool omitPadding)
{
    QByteArray encoded = payload.toBase64(QByteArray::Base64UrlEncoding);
    if (omitPadding) {
        while (encoded.endsWith('=')) {
            encoded.chop(1);
        }
    }
    return QStringLiteral("header.%1.signature").arg(QString::fromLatin1(encoded));
}

} // namespace

class HomeJsonTest : public QObject
{
    Q_OBJECT

private slots:
    void sessionFromUser();
    void sessionFromJwtSubject_data();
    void sessionFromJwtSubject();
    void invalidJwtExpiry_data();
    void invalidJwtExpiry();
    void invalidSessions_data();
    void invalidSessions();
    void errorMessages_data();
    void errorMessages();
    void profileRow();
    void invalidProfiles_data();
    void invalidProfiles();
    void switchableDevices();
    void thermometerDevice();
    void invalidDevices_data();
    void invalidDevices();
};

void HomeJsonTest::sessionFromUser()
{
    const QByteArray body = R"({"access_token":"access-1","refresh_token":"refresh-1","expires_in":3600,"user":{"id":"123e4567-e89b-12d3-a456-426614174000","email":"a@b.c"}})";
    const std::optional<SessionTokens> session = parseSession(body);
    QVERIFY(session.has_value());
    QCOMPARE(session->accessToken, QStringLiteral("access-1"));
    QCOMPARE(session->refreshToken, QStringLiteral("refresh-1"));
    QCOMPARE(session->expiresIn, 3600);
    QCOMPARE(session->userId, QStringLiteral("123e4567-e89b-12d3-a456-426614174000"));
    QCOMPARE(session->email, QStringLiteral("a@b.c"));
}

void HomeJsonTest::profileRow()
{
    const QByteArray body = R"([{"id":"123e4567-e89b-12d3-a456-426614174000","first_name":"Amina","city":"Nairobi"}])";
    const std::optional<ProfileRow> profile = parseProfile(body);
    QVERIFY(profile.has_value());
    QCOMPARE(profile->id, QStringLiteral("123e4567-e89b-12d3-a456-426614174000"));
    QCOMPARE(profile->firstName, QStringLiteral("Amina"));
    QCOMPARE(profile->city, QStringLiteral("Nairobi"));

    const std::optional<ProfileRow> noCity = parseProfile(QByteArrayLiteral(
        R"([{"id":"123e4567-e89b-12d3-a456-426614174000","first_name":"Amina","city":null}])"));
    QVERIFY(noCity.has_value());
    QVERIFY(noCity->city.isEmpty());
}

void HomeJsonTest::invalidProfiles_data()
{
    QTest::addColumn<QByteArray>("body");
    QTest::newRow("empty-array") << QByteArray("[]");
    QTest::newRow("object") << QByteArray("{}");
    QTest::newRow("non-object-row") << QByteArray("[1]");
    QTest::newRow("invalid-id") << QByteArray(R"([{"id":"bad","first_name":"A"}])");
    QTest::newRow("wrong-id-type") << QByteArray(R"([{"id":1,"first_name":"A"}])");
    QTest::newRow("missing-name") << QByteArray(R"([{"id":"123e4567-e89b-12d3-a456-426614174000"}])");
    QTest::newRow("empty-name") << QByteArray(R"([{"id":"123e4567-e89b-12d3-a456-426614174000","first_name":""}])");
    QTest::newRow("wrong-name-type") << QByteArray(R"([{"id":"123e4567-e89b-12d3-a456-426614174000","first_name":7}])");
    QTest::newRow("wrong-city-type") << QByteArray(R"([{"id":"123e4567-e89b-12d3-a456-426614174000","first_name":"A","city":7}])");
}

void HomeJsonTest::invalidProfiles()
{
    QFETCH(QByteArray, body);
    QVERIFY(!parseProfile(body).has_value());
}

void HomeJsonTest::switchableDevices()
{
    const QByteArray body = R"([
        {"id":"123e4567-e89b-12d3-a456-426614174010","room_id":"123e4567-e89b-12d3-a456-426614174000","name":"Lamp","kind":"light","is_on":true,"celsius":null,"reading_at":null,"position":2},
        {"id":"123e4567-e89b-12d3-a456-426614174011","room_id":"123e4567-e89b-12d3-a456-426614174000","name":"Fan","kind":"plug","is_on":false,"position":3}
    ])";
    const std::optional<QList<DeviceRow>> devices = parseDevices(body);
    QVERIFY(devices.has_value());
    QCOMPARE(devices->size(), 2);
    QCOMPARE(devices->at(0).id, QStringLiteral("123e4567-e89b-12d3-a456-426614174010"));
    QCOMPARE(devices->at(0).roomId, QStringLiteral("123e4567-e89b-12d3-a456-426614174000"));
    QCOMPARE(devices->at(0).name, QStringLiteral("Lamp"));
    QCOMPARE(devices->at(0).kind, QStringLiteral("light"));
    QVERIFY(devices->at(0).isOn.has_value());
    QCOMPARE(*devices->at(0).isOn, true);
    QVERIFY(!devices->at(0).celsius.has_value());
    QVERIFY(!devices->at(0).readingAt.isValid());
    QCOMPARE(devices->at(0).position, 2);
    QCOMPARE(devices->at(1).name, QStringLiteral("Fan"));
    QVERIFY(devices->at(1).isOn.has_value());
    QCOMPARE(*devices->at(1).isOn, false);
    QCOMPARE(devices->at(1).position, 3);
}

void HomeJsonTest::thermometerDevice()
{
    const QByteArray body = R"([{"id":"123e4567-e89b-12d3-a456-426614174010","room_id":"123e4567-e89b-12d3-a456-426614174000","name":"Probe","kind":"thermometer","is_on":null,"celsius":17.5,"reading_at":"2026-10-03T12:00:00.123456+00:00","position":1}])";
    const std::optional<QList<DeviceRow>> devices = parseDevices(body);
    QVERIFY(devices.has_value());
    QCOMPARE(devices->size(), 1);
    QCOMPARE(devices->at(0).id, QStringLiteral("123e4567-e89b-12d3-a456-426614174010"));
    QCOMPARE(devices->at(0).roomId, QStringLiteral("123e4567-e89b-12d3-a456-426614174000"));
    QCOMPARE(devices->at(0).name, QStringLiteral("Probe"));
    QCOMPARE(devices->at(0).kind, QStringLiteral("thermometer"));
    QVERIFY(!devices->at(0).isOn.has_value());
    QVERIFY(devices->at(0).celsius.has_value());
    QCOMPARE(*devices->at(0).celsius, 17.5);
    QCOMPARE(devices->at(0).readingAt,
        QDateTime::fromString(QStringLiteral("2026-10-03T12:00:00+00:00"), Qt::ISODate));
    QCOMPARE(devices->at(0).position, 1);
}

void HomeJsonTest::invalidDevices_data()
{
    QTest::addColumn<QByteArray>("body");
    QTest::addColumn<bool>("isArray");
    QTest::newRow("malformed") << QByteArray("not-json") << false;
    QTest::newRow("wrong-shape") << QByteArray("{}") << false;
    QTest::newRow("non-object-row") << QByteArray("[1]") << true;
    QTest::newRow("invalid-id") << QByteArray(R"([{"id":"bad","room_id":"123e4567-e89b-12d3-a456-426614174000","name":"Lamp","kind":"light","is_on":true}])") << true;
    QTest::newRow("invalid-room-id") << QByteArray(R"([{"id":"123e4567-e89b-12d3-a456-426614174010","room_id":7,"name":"Lamp","kind":"light","is_on":true}])") << true;
    QTest::newRow("unknown-kind") << QByteArray(R"([{"id":"123e4567-e89b-12d3-a456-426614174010","room_id":"123e4567-e89b-12d3-a456-426614174000","name":"Probe","kind":"thermometer","is_on":true}])") << true;
    QTest::newRow("missing-name") << QByteArray(R"([{"id":"123e4567-e89b-12d3-a456-426614174010","room_id":"123e4567-e89b-12d3-a456-426614174000","kind":"light","is_on":true}])") << true;
    QTest::newRow("wrong-state-type") << QByteArray(R"([{"id":"123e4567-e89b-12d3-a456-426614174010","room_id":"123e4567-e89b-12d3-a456-426614174000","name":"Lamp","kind":"light","is_on":1}])") << true;
    QTest::newRow("missing-state") << QByteArray(R"([{"id":"123e4567-e89b-12d3-a456-426614174010","room_id":"123e4567-e89b-12d3-a456-426614174000","name":"Lamp","kind":"light"}])") << true;
    QTest::newRow("sensor-value") << QByteArray(R"([{"id":"123e4567-e89b-12d3-a456-426614174010","room_id":"123e4567-e89b-12d3-a456-426614174000","name":"Lamp","kind":"light","is_on":true,"celsius":21.5}])") << true;
    QTest::newRow("sensor-timestamp") << QByteArray(R"([{"id":"123e4567-e89b-12d3-a456-426614174010","room_id":"123e4567-e89b-12d3-a456-426614174000","name":"Lamp","kind":"light","is_on":true,"reading_at":"2026-10-03T12:00:00Z"}])") << true;
    QTest::newRow("thermometer-null-celsius") << QByteArray(R"([{"id":"123e4567-e89b-12d3-a456-426614174010","room_id":"123e4567-e89b-12d3-a456-426614174000","name":"Probe","kind":"thermometer","is_on":null,"celsius":null,"reading_at":"2026-10-03T12:00:00Z"}])") << true;
    QTest::newRow("thermometer-missing-reading") << QByteArray(R"([{"id":"123e4567-e89b-12d3-a456-426614174010","room_id":"123e4567-e89b-12d3-a456-426614174000","name":"Probe","kind":"thermometer","is_on":null,"celsius":22.0}])") << true;
    QTest::newRow("thermometer-invalid-reading") << QByteArray(R"([{"id":"123e4567-e89b-12d3-a456-426614174010","room_id":"123e4567-e89b-12d3-a456-426614174000","name":"Probe","kind":"thermometer","is_on":null,"celsius":22.0,"reading_at":"not-a-date"}])") << true;
    QTest::newRow("thermometer-state-conflict") << QByteArray(R"([{"id":"123e4567-e89b-12d3-a456-426614174010","room_id":"123e4567-e89b-12d3-a456-426614174000","name":"Probe","kind":"thermometer","is_on":false,"celsius":22.0,"reading_at":"2026-10-03T12:00:00Z"}])") << true;
    QTest::newRow("thermometer-wrong-temperature-type") << QByteArray(R"([{"id":"123e4567-e89b-12d3-a456-426614174010","room_id":"123e4567-e89b-12d3-a456-426614174000","name":"Probe","kind":"thermometer","is_on":null,"celsius":"22","reading_at":"2026-10-03T12:00:00Z"}])") << true;
}

void HomeJsonTest::invalidDevices()
{
    QFETCH(QByteArray, body);
    QFETCH(bool, isArray);
    const std::optional<QList<DeviceRow>> devices = parseDevices(body);
    if (isArray) {
        QVERIFY(devices.has_value());
        QVERIFY(devices->isEmpty());
    } else {
        QVERIFY(!devices.has_value());
    }
}

void HomeJsonTest::sessionFromJwtSubject_data()
{
    QTest::addColumn<QString>("token");
    const QByteArray payload = R"({"sub":"123e4567-e89b-12d3-a456-426614174000","exp":1790000000,"n":12})";
    const QByteArray padded = payload.toBase64(QByteArray::Base64UrlEncoding);
    QVERIFY(padded.endsWith('='));
    QTest::newRow("padded") << tokenForPayload(payload, false);
    QTest::newRow("unpadded") << tokenForPayload(payload, true);
}

void HomeJsonTest::sessionFromJwtSubject()
{
    QFETCH(QString, token);
    const QByteArray body = QStringLiteral(
        R"({"access_token":"%1","refresh_token":"refresh-1","user":{}})").arg(token).toUtf8();
    const std::optional<SessionTokens> session = parseSession(body);
    QVERIFY(session.has_value());
    QCOMPARE(session->userId, QStringLiteral("123e4567-e89b-12d3-a456-426614174000"));
    QCOMPARE(jwtExpiryUtc(token), QDateTime::fromSecsSinceEpoch(1790000000, QTimeZone::utc()));
}

void HomeJsonTest::invalidJwtExpiry_data()
{
    QTest::addColumn<QString>("token");
    QTest::newRow("not-jwt") << QStringLiteral("invalid");
    QTest::newRow("malformed-payload") << tokenForPayload(QByteArrayLiteral("not-json"), true);
    QTest::newRow("missing-exp") << tokenForPayload(QByteArrayLiteral(R"({"sub":"id"})"), true);
    QTest::newRow("wrong-exp-type") << tokenForPayload(QByteArrayLiteral(R"({"exp":"1790000000"})"), true);
}

void HomeJsonTest::invalidJwtExpiry()
{
    QFETCH(QString, token);
    QVERIFY(!jwtExpiryUtc(token).isValid());
}

void HomeJsonTest::invalidSessions_data()
{
    QTest::addColumn<QByteArray>("body");
    QTest::newRow("malformed") << QByteArray("not-json");
    QTest::newRow("non-object") << QByteArray("[]");
    QTest::newRow("missing-access-token")
        << QByteArray(R"({"refresh_token":"r","user":{"id":"123e4567-e89b-12d3-a456-426614174000"}})");
    QTest::newRow("wrong-access-token-type")
        << QByteArray(R"({"access_token":1,"refresh_token":"r","user":{"id":"123e4567-e89b-12d3-a456-426614174000"}})");
    QTest::newRow("missing-refresh-token")
        << QByteArray(R"({"access_token":"a","user":{"id":"123e4567-e89b-12d3-a456-426614174000"}})");
    QTest::newRow("wrong-refresh-token-type")
        << QByteArray(R"({"access_token":"a","refresh_token":false,"user":{"id":"123e4567-e89b-12d3-a456-426614174000"}})");
    QTest::newRow("missing-user") << QByteArray(R"({"access_token":"a","refresh_token":"r"})");
    QTest::newRow("invalid-user-id")
        << QByteArray(R"({"access_token":"a","refresh_token":"r","user":{"id":"bad"}})");
    QTest::newRow("wrong-user-id-type")
        << QByteArray(R"({"access_token":"a","refresh_token":"r","user":{"id":7}})");
    const QString invalidSubjectToken = tokenForPayload(QByteArrayLiteral(R"({"sub":"not-a-uuid"})"), true);
    QTest::newRow("invalid-jwt-subject")
        << QStringLiteral(R"({"access_token":"%1","refresh_token":"r","user":{"id":"bad"}})")
               .arg(invalidSubjectToken).toUtf8();
}

void HomeJsonTest::invalidSessions()
{
    QFETCH(QByteArray, body);
    QVERIFY(!parseSession(body).has_value());
}

void HomeJsonTest::errorMessages_data()
{
    QTest::addColumn<QByteArray>("body");
    QTest::addColumn<QString>("expected");
    QTest::newRow("description-first")
        << QByteArray(R"({"error_description":"description","message":"message","msg":"msg","error":"error"})")
        << QStringLiteral("description");
    QTest::newRow("message-second")
        << QByteArray(R"({"error_description":"","message":"message","msg":"msg","error":"error"})")
        << QStringLiteral("message");
    QTest::newRow("msg-third")
        << QByteArray(R"({"error_description":null,"message":42,"msg":"msg","error":"error"})")
        << QStringLiteral("msg");
    QTest::newRow("error-fallback") << QByteArray(R"({"msg":"","error":"invalid_grant"})")
        << QStringLiteral("invalid_grant");
    QTest::newRow("wrong-types") << QByteArray(R"({"error_description":[],"message":{},"msg":false,"error":1})") << QString();
    QTest::newRow("missing-fields") << QByteArray("{}") << QString();
    QTest::newRow("array") << QByteArray("[]") << QString();
    QTest::newRow("malformed-json") << QByteArray("not-json") << QString();
    QTest::newRow("empty-body") << QByteArray() << QString();
}

void HomeJsonTest::errorMessages()
{
    QFETCH(QByteArray, body);
    QFETCH(QString, expected);
    QCOMPARE(parseErrorMessage(body), expected);
}

QTEST_GUILESS_MAIN(HomeJsonTest)
#include "tst_home_json.moc"