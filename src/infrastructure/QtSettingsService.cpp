#include "infrastructure/QtSettingsService.h"

QtSettingsService::QtSettingsService(const QString& settingsFilePath)
    : settings_(settingsFilePath, QSettings::IniFormat)
{
}

QVariant QtSettingsService::value(
    const QString& key,
    const QVariant& defaultValue) const
{
    return settings_.value(key, defaultValue);
}

void QtSettingsService::setValue(const QString& key, const QVariant& value)
{
    settings_.setValue(key, value);
}

void QtSettingsService::sync()
{
    settings_.sync();
}
