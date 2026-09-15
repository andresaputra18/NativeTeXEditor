#pragma once

#include "services/IProjectService.h"

class ProjectService final : public IProjectService
{
public:
    [[nodiscard]] bool openProject(
        const QString& rootPath,
        QString& errorMessage) override;

    void closeProject() override;

    [[nodiscard]] bool isOpen() const noexcept override;
    [[nodiscard]] QString rootPath() const override;
    [[nodiscard]] QString displayName() const override;

    [[nodiscard]] bool containsPath(const QString& path) const override;
    [[nodiscard]] QString relativePath(const QString& path) const override;

    [[nodiscard]] bool setMainDocumentPath(const QString& filePath) override;
    void clearMainDocumentPath() override;
    [[nodiscard]] QString mainDocumentPath() const override;

private:
    [[nodiscard]] static QString normalizedExistingPath(const QString& path);

    QString rootPath_;
    QString mainDocumentPath_;
};
