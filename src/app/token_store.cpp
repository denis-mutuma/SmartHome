#include "token_store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincrypt.h>
#endif

namespace {

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

} // namespace

QString sessionFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/session.bin");
}

bool saveRefreshToken(const QString& token, const QString& filePath)
{
    if (token.isEmpty()) {
        return clearRefreshToken(filePath);
    }
    const QByteArray plain = token.toUtf8();
#ifdef Q_OS_WIN
    const QByteArray sealed = protect(plain);
    if (sealed.isEmpty()) {
        return false;
    }
    return writeBytes(filePath, sealed);
#else
    return writeBytes(filePath, plain);
#endif
}

QString loadRefreshToken(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QByteArray bytes = file.readAll();
#ifdef Q_OS_WIN
    return QString::fromUtf8(unprotect(bytes));
#else
    return QString::fromUtf8(bytes);
#endif
}

bool clearRefreshToken(const QString& filePath)
{
    if (!QFile::exists(filePath)) {
        return true;
    }
    return QFile::remove(filePath);
}
