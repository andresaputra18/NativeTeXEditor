#include "app/ApplicationPaths.h"

#include <QDir>
#include <QStandardPaths>
#include <QStringList>

ApplicationPaths ApplicationPaths::create()
{
    ApplicationPaths paths;

    paths.dataDirectory =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    paths.cacheDirectory =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    paths.logDirectory = QDir(paths.dataDirectory).filePath("logs");
    paths.recoveryDirectory =
        QDir(paths.dataDirectory).filePath("recovery");
    paths.logFilePath =
        QDir(paths.logDirectory).filePath("NativeTeXEditor.log");
    paths.settingsFilePath =
        QDir(paths.dataDirectory).filePath("settings.ini");

    return paths;
}

bool ApplicationPaths::ensureDirectories(QString* errorMessage) const
{
    if (dataDirectory.isEmpty()
        || cacheDirectory.isEmpty()
        || logDirectory.isEmpty()
        || recoveryDirectory.isEmpty()) {
        if (errorMessage)
            *errorMessage =
                "Qt did not provide writable application directories.";
        return false;
    }

    const QStringList directories{
        dataDirectory,
        cacheDirectory,
        logDirectory,
        recoveryDirectory
    };

    for (const QString& directory : directories) {
        if (!QDir().mkpath(directory)) {
            if (errorMessage) {
                *errorMessage =
                    QString("Unable to create application directory: %1")
                        .arg(directory);
            }
            return false;
        }
    }

    return true;
}
