#pragma once

#include <QtGlobal>
#include <QString>

enum class TeXEngine
{
    PdfLaTeX,
    LuaLaTeX,
    XeLaTeX
};

enum class TeXDistribution
{
    Unknown,
    MiKTeX,
    TeXLive
};

struct BuildRequest final
{
    QString targetPath;
    TeXEngine engine = TeXEngine::PdfLaTeX;

    // Overall build budget. Zero disables automatic timeout.
    int timeoutMilliseconds = 10 * 60 * 1000;
};

struct BuildResult final
{
    bool started = false;
    bool success = false;
    bool cancelled = false;
    bool timedOut = false;
    bool crashed = false;

    int exitCode = -1;
    qint64 elapsedMilliseconds = 0;

    QString targetPath;
    QString pdfPath;
    QString output;
    QString errorMessage;
};

[[nodiscard]] QString texEngineDisplayName(TeXEngine engine);
[[nodiscard]] QString texEngineSettingValue(TeXEngine engine);
[[nodiscard]] TeXEngine texEngineFromSettingValue(const QString& value);
[[nodiscard]] QString texDistributionDisplayName(
    TeXDistribution distribution);
