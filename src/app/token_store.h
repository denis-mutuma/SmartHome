#pragma once

#include <QString>

QString sessionFilePath();
bool saveRefreshToken(const QString& token, const QString& filePath);
QString loadRefreshToken(const QString& filePath);
bool clearRefreshToken(const QString& filePath);