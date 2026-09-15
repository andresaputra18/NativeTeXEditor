#include "infrastructure/EnvironmentService.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

EnvironmentService::EnvironmentService()
{
    refresh();
}

void EnvironmentService::setCustomToolchainPath(
    const QString& directory)
{
    if (directory.trimmed().isEmpty()) {
        customToolchainPath_.clear();
    } else {
        customToolchainPath_ =
            QDir::cleanPath(
                QFileInfo(directory).absoluteFilePath());
    }

    refresh();
}

QString EnvironmentService::customToolchainPath() const
{
    return customToolchainPath_;
}

ToolInfo EnvironmentService::findTool(
    const QString& displayName,
    const QString& executableName) const
{
    ToolInfo result;
    result.displayName = displayName;
    result.executableName = executableName;

    if (!customToolchainPath_.isEmpty()) {
        result.path =
            QStandardPaths::findExecutable(
                executableName,
                QStringList{customToolchainPath_});
    }

    if (result.path.isEmpty()) {
        result.path =
            QStandardPaths::findExecutable(
                executableName);
    }

    if (!result.path.isEmpty()) {
        result.path =
            QDir::cleanPath(
                QFileInfo(result.path).absoluteFilePath());
    }

    return result;
}

TeXDistribution EnvironmentService::detectDistribution() const
{
    if (snapshot_.initexmf.available()
        || snapshot_.miktex.available()) {
        return TeXDistribution::MiKTeX;
    }

    if (snapshot_.tlmgr.available()) {
        return TeXDistribution::TeXLive;
    }

    const auto pathContains =
        [](const ToolInfo& tool, const QString& token) {
            return tool.available()
                && tool.path.contains(
                    token,
                    Qt::CaseInsensitive);
        };

    if (pathContains(snapshot_.pdflatex, "miktex")
        || pathContains(snapshot_.lualatex, "miktex")
        || pathContains(snapshot_.xelatex, "miktex")) {
        return TeXDistribution::MiKTeX;
    }

    if (pathContains(snapshot_.pdflatex, "texlive")
        || pathContains(snapshot_.lualatex, "texlive")
        || pathContains(snapshot_.xelatex, "texlive")
        || snapshot_.kpsewhich.available()) {
        return TeXDistribution::TeXLive;
    }

    return TeXDistribution::Unknown;
}

void EnvironmentService::refresh()
{
    snapshot_.customToolchainPath =
        customToolchainPath_;

    snapshot_.pdflatex =
        findTool("pdfLaTeX", "pdflatex");
    snapshot_.lualatex =
        findTool("LuaLaTeX", "lualatex");
    snapshot_.xelatex =
        findTool("XeLaTeX", "xelatex");
    snapshot_.bibtex =
        findTool("BibTeX", "bibtex");
    snapshot_.biber =
        findTool("Biber", "biber");
    snapshot_.synctex =
        findTool("SyncTeX", "synctex");

    snapshot_.latexmk =
        findTool("latexmk (optional)", "latexmk");
    snapshot_.perl =
        findTool("Perl (optional)", "perl");

    snapshot_.initexmf =
        findTool("MiKTeX initexmf", "initexmf");
    snapshot_.miktex =
        findTool("MiKTeX CLI", "miktex");
    snapshot_.kpsewhich =
        findTool("kpsewhich", "kpsewhich");
    snapshot_.tlmgr =
        findTool("TeX Live Manager", "tlmgr");

    snapshot_.distribution =
        detectDistribution();
}

TeXEnvironmentSnapshot EnvironmentService::snapshot() const
{
    return snapshot_;
}

bool EnvironmentService::canBuildWith(
    TeXEngine engine) const
{
    return !engineExecutablePath(engine).isEmpty();
}

QString EnvironmentService::engineExecutablePath(
    TeXEngine engine) const
{
    switch (engine) {
    case TeXEngine::PdfLaTeX:
        return snapshot_.pdflatex.path;
    case TeXEngine::LuaLaTeX:
        return snapshot_.lualatex.path;
    case TeXEngine::XeLaTeX:
        return snapshot_.xelatex.path;
    }

    return {};
}
