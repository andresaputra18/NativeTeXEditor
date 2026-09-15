#pragma once

#include <QString>

struct PreviewGeneration final
{
    QString id;
    QString buildTargetPath;
    QString originalPdfPath;
    QString originalBuildDirectory;
    QString cacheDirectory;
    QString cachedPdfPath;
    QString cachedSyncTeXPath;

    [[nodiscard]] bool hasSyncTeX() const noexcept
    {
        return !cachedSyncTeXPath.isEmpty();
    }
};
