#pragma once

#include "build/BuildTypes.h"
#include "pdf/PdfTypes.h"
#include "preview/PreviewTypes.h"

#include <QString>

#include <functional>
#include <optional>

class QWidget;

class IPreviewService
{
public:
    using InverseSyncCallback =
        std::function<void(const PdfPosition&)>;

    virtual ~IPreviewService() = default;

    [[nodiscard]] virtual QWidget* widget() = 0;
    [[nodiscard]] virtual bool activateSuccessfulBuild(
        const BuildResult& result,
        QString& errorMessage) = 0;
    virtual void clear() = 0;
    virtual void clearIfTargetChanged(
        const QString& targetPath) = 0;
    [[nodiscard]] virtual std::optional<PreviewGeneration>
        currentGeneration() const = 0;
    virtual void navigateTo(const PdfPosition& position) = 0;
    virtual void setInverseSyncCallback(
        InverseSyncCallback callback) = 0;
};
