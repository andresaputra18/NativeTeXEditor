#pragma once

#include <QDateTime>
#include <QString>

#include <optional>

struct RecoverySnapshot final
{
    QString originalFilePath;
    QString text;
    QDateTime timestampUtc;
};

class IRecoveryService
{
public:
    virtual ~IRecoveryService() = default;

    [[nodiscard]] virtual bool hasSnapshot() const = 0;

    [[nodiscard]] virtual bool writeSnapshot(
        const QString& originalFilePath,
        const QString& text,
        QString& errorMessage) = 0;

    [[nodiscard]] virtual std::optional<RecoverySnapshot> loadSnapshot(
        QString& errorMessage) const = 0;

    [[nodiscard]] virtual bool discardSnapshot(
        QString& errorMessage) = 0;
};
