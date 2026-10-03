#include "home_json.h"

#include <QTest>

class HomeJsonTest : public QObject
{
    Q_OBJECT

private slots:
    void sessionFromUser();
    void invalidSessions_data();
    void invalidSessions();
    void errorMessages_data();
    void errorMessages();
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