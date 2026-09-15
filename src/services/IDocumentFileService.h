#pragma once

#include <QString>

class IDocumentFileService
{
public:
    virtual ~IDocumentFileService() = default;

    [[nodiscard]] virtual bool readUtf8(
        const QString& filePath,
        QString& text,
        QString& errorMessage) const = 0;

    [[nodiscard]] virtual bool writeUtf8Atomic(
        const QString& filePath,
        const QString& text,
        QString& errorMessage) const = 0;
};
