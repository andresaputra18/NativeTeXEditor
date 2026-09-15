#pragma once

#include "services/IDocumentFileService.h"

class QtDocumentFileService final : public IDocumentFileService
{
public:
    [[nodiscard]] bool readUtf8(
        const QString& filePath,
        QString& text,
        QString& errorMessage) const override;

    [[nodiscard]] bool writeUtf8Atomic(
        const QString& filePath,
        const QString& text,
        QString& errorMessage) const override;
};
