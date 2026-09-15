#pragma once

#include "services/IPreviewService.h"

#include <QString>

class IPdfBackend;

class PreviewService final : public IPreviewService
{
public:
    PreviewService(
        const QString& applicationCacheDirectory,
        IPdfBackend& pdfBackend);
    ~PreviewService() override;

    [[nodiscard]] QWidget* widget() override;
    [[nodiscard]] bool activateSuccessfulBuild(
        const BuildResult& result,
        QString& errorMessage) override;
    void clear() override;
    void clearIfTargetChanged(const QString& targetPath) override;
    [[nodiscard]] std::optional<PreviewGeneration>
        currentGeneration() const override;
    void navigateTo(const PdfPosition& position) override;
    void setInverseSyncCallback(InverseSyncCallback callback) override;

private:
    [[nodiscard]] QString matchingSyncTeXPath(
        const QString& pdfPath) const;
    [[nodiscard]] bool copyGenerationFile(
        const QString& sourcePath,
        const QString& destinationPath,
        QString& errorMessage) const;
    void removeGenerationDirectory(const QString& directoryPath) const;
    [[nodiscard]] static QString normalizedPath(const QString& path);

    QString previewCacheRoot_;
    IPdfBackend& pdfBackend_;
    std::optional<PreviewGeneration> currentGeneration_;
};
