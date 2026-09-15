#include "project/ProjectService.h"

#include <QDir>
#include <QFileInfo>

QString ProjectService::normalizedExistingPath(const QString& path)
{
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();

    if (!canonical.isEmpty())
        return QDir::cleanPath(canonical);

    return QDir::cleanPath(info.absoluteFilePath());
}

bool ProjectService::openProject(
    const QString& rootPath,
    QString& errorMessage)
{
    const QFileInfo info(rootPath);

    if (!info.exists()) {
        errorMessage =
            QString("The project folder does not exist:\n%1")
                .arg(QDir::toNativeSeparators(rootPath));
        return false;
    }

    if (!info.isDir()) {
        errorMessage =
            QString("The selected project path is not a folder:\n%1")
                .arg(QDir::toNativeSeparators(rootPath));
        return false;
    }

    if (!info.isReadable()) {
        errorMessage =
            QString("The project folder is not readable:\n%1")
                .arg(QDir::toNativeSeparators(rootPath));
        return false;
    }

    rootPath_ = normalizedExistingPath(rootPath);
    mainDocumentPath_.clear();
    return true;
}

void ProjectService::closeProject()
{
    rootPath_.clear();
    mainDocumentPath_.clear();
}

bool ProjectService::isOpen() const noexcept
{
    return !rootPath_.isEmpty();
}

QString ProjectService::rootPath() const
{
    return rootPath_;
}

QString ProjectService::displayName() const
{
    if (rootPath_.isEmpty())
        return {};

    const QString name = QFileInfo(rootPath_).fileName();

    if (!name.isEmpty())
        return name;

    return QDir::toNativeSeparators(rootPath_);
}

bool ProjectService::containsPath(const QString& path) const
{
    if (!isOpen() || path.isEmpty())
        return false;

    const QString normalized = normalizedExistingPath(path);
    const QString relative = QDir(rootPath_).relativeFilePath(normalized);

    if (relative == ".")
        return true;

    if (QDir::isAbsolutePath(relative))
        return false;

    return relative != ".."
        && !relative.startsWith("../")
        && !relative.startsWith("..\\");
}

QString ProjectService::relativePath(const QString& path) const
{
    if (!containsPath(path))
        return {};

    return QDir(rootPath_).relativeFilePath(normalizedExistingPath(path));
}

bool ProjectService::setMainDocumentPath(const QString& filePath)
{
    const QFileInfo info(filePath);

    if (!info.exists() || !info.isFile() || !containsPath(filePath))
        return false;

    mainDocumentPath_ = normalizedExistingPath(filePath);
    return true;
}

void ProjectService::clearMainDocumentPath()
{
    mainDocumentPath_.clear();
}

QString ProjectService::mainDocumentPath() const
{
    return mainDocumentPath_;
}
