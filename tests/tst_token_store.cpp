#include "token_store.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class TokenStoreTest : public QObject
{
    Q_OBJECT

private slots:
    void roundTrip();
    void missingAndGarbage();
};

void TokenStoreTest::roundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("session.bin"));
    QVERIFY(saveRefreshToken(QStringLiteral("refresh-token-value"), path));
    QCOMPARE(loadRefreshToken(path), QStringLiteral("refresh-token-value"));
    QVERIFY(clearRefreshToken(path));
    QVERIFY(loadRefreshToken(path).isEmpty());
    QVERIFY(saveRefreshToken(QStringLiteral("refresh-token-value"), path));
    QVERIFY(saveRefreshToken(QString(), path));
    QVERIFY(!QFile::exists(path));
}

void TokenStoreTest::missingAndGarbage()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("session.bin"));
    QVERIFY(loadRefreshToken(path).isEmpty());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write("not-a-sealed-token") > 0);
    file.close();
#ifdef Q_OS_WIN
    QVERIFY(loadRefreshToken(path).isEmpty());
#else
    QCOMPARE(loadRefreshToken(path), QStringLiteral("not-a-sealed-token"));
#endif
}

QTEST_GUILESS_MAIN(TokenStoreTest)
#include "tst_token_store.moc"
