#include "home_rules.h"

#include <QTest>

class HomeRulesTest : public QObject
{
    Q_OBJECT

private slots:
    void greetingBands();
    void deviceCreationKinds();
    void fieldLimits();
};

void HomeRulesTest::greetingBands()
{
    QCOMPARE(greetingFor(QTime(4, 59)), QStringLiteral("Good night,"));
    QCOMPARE(greetingFor(QTime(5, 0)), QStringLiteral("Good morning,"));
    QCOMPARE(greetingFor(QTime(11, 59)), QStringLiteral("Good morning,"));
    QCOMPARE(greetingFor(QTime(12, 0)), QStringLiteral("Good afternoon,"));
    QCOMPARE(greetingFor(QTime(16, 59)), QStringLiteral("Good afternoon,"));
    QCOMPARE(greetingFor(QTime(17, 0)), QStringLiteral("Good evening,"));
    QCOMPARE(greetingFor(QTime(20, 59)), QStringLiteral("Good evening,"));
    QCOMPARE(greetingFor(QTime(21, 0)), QStringLiteral("Good night,"));
    QCOMPARE(greetingFor(QTime(0, 0)), QStringLiteral("Good night,"));
}

void HomeRulesTest::deviceCreationKinds()
{
    QVERIFY(isDeviceKindCreatable(QStringLiteral("light")));
    QVERIFY(isDeviceKindCreatable(QStringLiteral("plug")));
    QVERIFY(!isDeviceKindCreatable(QStringLiteral("thermometer")));
    QVERIFY(!isDeviceKindCreatable(QStringLiteral("unknown")));
}

void HomeRulesTest::fieldLimits()
{
    QVERIFY(isNameOk(QStringLiteral("Kitchen")));
    QVERIFY(!isNameOk(QString()));
    QVERIFY(!isNameOk(QString(41, QLatin1Char('a'))));
    QVERIFY(isPasswordOk(QStringLiteral("12345678")));
    QVERIFY(!isPasswordOk(QStringLiteral("short")));
    QVERIFY(!isPasswordOk(QString(73, QLatin1Char('a'))));
    QVERIFY(isEmailOk(QStringLiteral("a@b.c")));
    QVERIFY(!isEmailOk(QStringLiteral("not-an-email")));
    QVERIFY(isCityOk(QString()));
    QVERIFY(isCityOk(QStringLiteral("Nairobi")));
    QVERIFY(!isCityOk(QString(81, QLatin1Char('a'))));
    QVERIFY(isUuid(QStringLiteral("123e4567-e89b-12d3-a456-426614174000")));
    QVERIFY(!isUuid(QStringLiteral("not-a-uuid")));
}

QTEST_GUILESS_MAIN(HomeRulesTest)
#include "tst_home_rules.moc"
