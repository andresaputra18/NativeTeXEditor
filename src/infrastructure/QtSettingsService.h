#pragma once

#include "services/ISettingsService.h"

#include <QSettings>

class QtSettingsService final : public ISettingsService
{
public:
    explicit QtSettingsService(const QString& settingsFilePath);

    [[nodiscard]] QVariant value(
        const QString& key,
        const QVariant& defaultValue = {}) const override;

    void setValue(const QString& key, const QVariant& value) override;
    void sync() override;

private:
    mutable QSettings settings_;
};
