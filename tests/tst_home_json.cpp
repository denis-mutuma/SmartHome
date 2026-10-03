#include "home_json.h"

#include <QTest>

class HomeJsonTest : public QObject
{
    Q_OBJECT

private slots:
    void errorMessages_data();
    void errorMessages();
};

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