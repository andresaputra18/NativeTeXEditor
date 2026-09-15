#pragma once

#include "services/IRecoveryService.h"

class RecoveryService final : public IRecoveryService
{
public:
    explicit RecoveryService(const QString& recoveryDirectory);

    [[nodiscard]] bool hasSnapshot() const override;

    [[nodiscard]] bool writeSnapshot(
        const QString& originalFilePath,
        const QString& text,
        QString& errorMessage) override;

    [[nodiscard]] std::optional<RecoverySnapshot> loadSnapshot(
        QString& errorMessage) const override;

    [[nodiscard]] bool discardSnapshot(
        QString& errorMessage) override;

private:
    QString snapshotFilePath_;
};
