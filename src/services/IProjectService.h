#pragma once

#include <QString>

class IProjectService
{
public:
    virtual ~IProjectService() = default;

    [[nodiscard]] virtual bool openProject(
        const QString& rootPath,
        QString& errorMessage) = 0;

    virtual void closeProject() = 0;

    [[nodiscard]] virtual bool isOpen() const noexcept = 0;
    [[nodiscard]] virtual QString rootPath() const = 0;
    [[nodiscard]] virtual QString displayName() const = 0;

    [[nodiscard]] virtual bool containsPath(const QString& path) const = 0;
    [[nodiscard]] virtual QString relativePath(const QString& path) const = 0;

    [[nodiscard]] virtual bool setMainDocumentPath(const QString& filePath) = 0;
    virtual void clearMainDocumentPath() = 0;
    [[nodiscard]] virtual QString mainDocumentPath() const = 0;
};
