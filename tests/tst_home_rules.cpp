#include "home_rules.h"

#include <QTest>

class HomeRulesTest : public QObject
{
    Q_OBJECT

private slots:
    void greetingBands();
    void celsiusClamp();
    void staleReading();
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

void HomeRulesTest::celsiusClamp()
{
    QCOMPARE(clampCelsius(17.9), 18.0);
    QCOMPARE(clampCelsius(28.1), 28.0);
    QCOMPARE(clampCelsius(22.04), 22.0);
    QCOMPARE(clampCelsius(22.05), 22.1);
    QCOMPARE(nextCelsius(28.0, 0.2), 28.0);
    QCOMPARE(nextCelsius(18.0, -0.2), 18.0);
}

void HomeRulesTest::staleReading()
{
    const QDateTime now = QDateTime::fromString(QStringLiteral("2026-09-29T12:00:00Z"), Qt::ISODate);
    QVERIFY(readingIsStale(QDateTime(), now));
    QVERIFY(readingIsStale(now.addSecs(-15 * 60), now));
    QVERIFY(!readingIsStale(now.addSecs(-(15 * 60 - 1)), now));
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
