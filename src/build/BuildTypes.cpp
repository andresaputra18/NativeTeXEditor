#include "build/BuildTypes.h"

QString texEngineDisplayName(TeXEngine engine)
{
    switch (engine) {
    case TeXEngine::PdfLaTeX:
        return "pdfLaTeX";
    case TeXEngine::LuaLaTeX:
        return "LuaLaTeX";
    case TeXEngine::XeLaTeX:
        return "XeLaTeX";
    }

    return "pdfLaTeX";
}

QString texEngineSettingValue(TeXEngine engine)
{
    switch (engine) {
    case TeXEngine::PdfLaTeX:
        return "pdflatex";
    case TeXEngine::LuaLaTeX:
        return "lualatex";
    case TeXEngine::XeLaTeX:
        return "xelatex";
    }

    return "pdflatex";
}

TeXEngine texEngineFromSettingValue(const QString& value)
{
    const QString normalized = value.trimmed().toLower();

    if (normalized == "lualatex")
        return TeXEngine::LuaLaTeX;

    if (normalized == "xelatex")
        return TeXEngine::XeLaTeX;

    return TeXEngine::PdfLaTeX;
}

QString texDistributionDisplayName(
    TeXDistribution distribution)
{
    switch (distribution) {
    case TeXDistribution::MiKTeX:
        return "MiKTeX";
    case TeXDistribution::TeXLive:
        return "TeX Live";
    case TeXDistribution::Unknown:
        return "Unknown";
    }

    return "Unknown";
}
