#include "token_store.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class TokenStoreTest : public QObject
{
    Q_OBJECT

private slots:
    void roundTripAndClear();
    void missingAndCorrupt();
#ifdef Q_OS_ANDROID
    void migratesLegacyPlaintext();
#endif
};

void TokenStoreTest::roundTripAndClear()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("session.bin"));
    QVERIFY(saveRefreshToken(QStringLiteral("refresh-token-value"), path));
    QCOMPARE(loadRefreshToken(path), QStringLiteral("refresh-token-value"));
#ifdef Q_OS_ANDROID
    QFile encrypted(path);
    QVERIFY(encrypted.open(QIODevice::ReadOnly));
    const QByteArray stored = encrypted.readAll();
    QVERIFY(stored.startsWith("android:v1:"));
    QVERIFY(!stored.contains("refresh-token-value"));
#endif
    QVERIFY(clearRefreshToken(path));
    QVERIFY(loadRefreshToken(path).isEmpty());
    QVERIFY(saveRefreshToken(QStringLiteral("refresh-token-value"), path));
    QVERIFY(saveRefreshToken(QString(), path));
    QVERIFY(!QFile::exists(path));
}

void TokenStoreTest::missingAndCorrupt()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("session.bin"));
    QVERIFY(loadRefreshToken(path).isEmpty());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
#ifdef Q_OS_ANDROID
    QVERIFY(file.write("android:v1:invalid-ciphertext") > 0);
#else
    QVERIFY(file.write("not-a-sealed-token") > 0);
#endif
    file.close();
#ifdef Q_OS_WIN
    QVERIFY(loadRefreshToken(path).isEmpty());
#elif defined(Q_OS_ANDROID)
    QVERIFY(loadRefreshToken(path).isEmpty());
    QVERIFY(!QFile::exists(path));

    QFile futureVersion(path);
    QVERIFY(futureVersion.open(QIODevice::WriteOnly));
    QVERIFY(futureVersion.write("android:v2:future-format") > 0);
    futureVersion.close();
    QVERIFY(loadRefreshToken(path).isEmpty());
    QVERIFY(!QFile::exists(path));
#else
    QCOMPARE(loadRefreshToken(path), QStringLiteral("not-a-sealed-token"));
#endif
}

#ifdef Q_OS_ANDROID
void TokenStoreTest::migratesLegacyPlaintext()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("session.bin"));
    const QString token = QStringLiteral("legacy-refresh-token");
    QFile legacy(path);
    QVERIFY(legacy.open(QIODevice::WriteOnly));
    QCOMPARE(legacy.write(token.toUtf8()), token.toUtf8().size());
    legacy.close();

    QCOMPARE(loadRefreshToken(path), token);
    QFile encrypted(path);
    QVERIFY(encrypted.open(QIODevice::ReadOnly));
    const QByteArray stored = encrypted.readAll();
    QVERIFY(stored.startsWith("android:v1:"));
    QVERIFY(!stored.contains(token.toUtf8()));
}
#endif

QTEST_GUILESS_MAIN(TokenStoreTest)
#include "tst_token_store.moc"
