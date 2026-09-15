#pragma once

#include <QString>

struct ApplicationPaths final
{
    QString dataDirectory;
    QString cacheDirectory;
    QString logDirectory;
    QString recoveryDirectory;
    QString logFilePath;
    QString settingsFilePath;

    [[nodiscard]] static ApplicationPaths create();
    [[nodiscard]] bool ensureDirectories(QString* errorMessage = nullptr) const;
};
