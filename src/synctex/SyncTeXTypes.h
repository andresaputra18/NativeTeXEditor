#pragma once

#include "pdf/PdfTypes.h"
#include "preview/PreviewTypes.h"

#include <QtGlobal>
#include <QString>

struct SourcePosition final
{
    QString filePath;
    int line = 0;
    int column = 0;

    [[nodiscard]] bool isValid() const noexcept
    {
        return !filePath.isEmpty() && line > 0;
    }
};

struct SyncTeXResult final
{
    bool success = false;
    PdfPosition pdfPosition;
    SourcePosition sourcePosition;
    qint64 elapsedMilliseconds = 0;
    QString rawOutput;
    QString errorMessage;
};

struct SyncTeXRequestContext final
{
    PreviewGeneration generation;
    QString executablePath;
};
