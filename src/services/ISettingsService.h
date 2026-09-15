#pragma once

#include <QString>
#include <QVariant>

class ISettingsService
{
public:
    virtual ~ISettingsService() = default;

    [[nodiscard]] virtual QVariant value(
        const QString& key,
        const QVariant& defaultValue = {}) const = 0;

    virtual void setValue(const QString& key, const QVariant& value) = 0;
    virtual void sync() = 0;
};
