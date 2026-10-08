#include "token_store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#endif

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincrypt.h>
#endif

namespace {

#if defined(Q_OS_WIN) || defined(Q_OS_ANDROID)
bool writeBytes(const QString& filePath, const QByteArray& bytes)
{
    const QFileInfo info(filePath);
    if (!QDir().mkpath(info.absolutePath())) {
        return false;
    }
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    if (file.write(bytes) != bytes.size()) {
        return false;
    }
    return file.commit();
}
#endif

#ifdef Q_OS_WIN
QByteArray protect(const QByteArray& plain)
{
    if (plain.size() > static_cast<qsizetype>(DWORD(-1))) {
        return {};
    }
    DATA_BLOB input;
    input.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plain.constData()));
    input.cbData = static_cast<DWORD>(plain.size());
    DATA_BLOB output{};
    if (!CryptProtectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        return {};
    }
    const QByteArray sealed(reinterpret_cast<const char*>(output.pbData), static_cast<int>(output.cbData));
    LocalFree(output.pbData);
    return sealed;
}

QByteArray unprotect(const QByteArray& sealed)
{
    if (sealed.isEmpty() || sealed.size() > static_cast<qsizetype>(DWORD(-1))) {
        return {};
    }
    DATA_BLOB input;
    input.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(sealed.constData()));
    input.cbData = static_cast<DWORD>(sealed.size());
    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        return {};
    }
    const QByteArray plain(reinterpret_cast<const char*>(output.pbData), static_cast<int>(output.cbData));
    LocalFree(output.pbData);
    return plain;
}
#endif

#ifdef Q_OS_ANDROID
const QByteArray androidTokenPrefix = QByteArrayLiteral("android:v1:");

QString encryptAndroidToken(const QString& token)
{
    const QJniObject javaToken = QJniObject::fromString(token);
    const QJniObject encrypted = QJniObject::callStaticObjectMethod(
        "org/mutuma/smarthome/TokenVault", "encrypt", "(Ljava/lang/String;)Ljava/lang/String;",
        javaToken.object<jstring>());
    return encrypted.isValid() ? encrypted.toString() : QString();
}

QString decryptAndroidToken(const QString& ciphertext)
{
    const QJniObject javaCiphertext = QJniObject::fromString(ciphertext);
    const QJniObject decrypted = QJniObject::callStaticObjectMethod(
        "org/mutuma/smarthome/TokenVault", "decrypt", "(Ljava/lang/String;)Ljava/lang/String;",
        javaCiphertext.object<jstring>());
    return decrypted.isValid() ? decrypted.toString() : QString();
}

bool clearAndroidTokenKey()
{
    return QJniObject::callStaticMethod<jboolean>("org/mutuma/smarthome/TokenVault", "clearKey");
}
#endif

} // namespace

QString sessionFilePath()
{
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return appData.isEmpty() ? QString() : QDir(appData).filePath(QStringLiteral("session.bin"));
}

bool saveRefreshToken(const QString& token, const QString& filePath)
{
    if (token.isEmpty()) {
        return clearRefreshToken(filePath);
    }
#ifdef Q_OS_WIN
    const QByteArray plain = token.toUtf8();
    const QByteArray sealed = protect(plain);
    if (sealed.isEmpty()) {
        return false;
    }
    return writeBytes(filePath, sealed);
#elif defined(Q_OS_ANDROID)
    const QString ciphertext = encryptAndroidToken(token);
    if (ciphertext.isEmpty()) {
        return false;
    }
    return writeBytes(filePath, androidTokenPrefix + ciphertext.toLatin1());
#else
    return false;
#endif
}

QString loadRefreshToken(const QString& filePath)
{
#if defined(Q_OS_WIN) || defined(Q_OS_ANDROID)
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QByteArray bytes = file.readAll();
#ifdef Q_OS_WIN
    return QString::fromUtf8(unprotect(bytes));
#elif defined(Q_OS_ANDROID)
    if (bytes.startsWith("android:") && !bytes.startsWith(androidTokenPrefix)) {
        clearRefreshToken(filePath);
        return {};
    }
    if (bytes.startsWith(androidTokenPrefix)) {
        const QString token = decryptAndroidToken(QString::fromLatin1(bytes.mid(androidTokenPrefix.size())));
        if (token.isEmpty()) {
            clearRefreshToken(filePath);
        }
        return token;
    }

    const QString legacyToken = QString::fromUtf8(bytes);
    if (legacyToken.isEmpty() || legacyToken.toUtf8() != bytes) {
        clearRefreshToken(filePath);
        return {};
    }
    if (saveRefreshToken(legacyToken, filePath)) {
        return legacyToken;
    }
    clearRefreshToken(filePath);
    return {};
#endif
#else
    Q_UNUSED(filePath);
    return {};
#endif
}

bool clearRefreshToken(const QString& filePath)
{
    const bool fileRemoved = !QFile::exists(filePath) || QFile::remove(filePath);
#ifdef Q_OS_ANDROID
    return clearAndroidTokenKey() && fileRemoved;
#else
    return fileRemoved;
#endif
}
