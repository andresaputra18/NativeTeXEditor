#pragma once

#include "build/BuildTypes.h"

#include <QString>

struct ToolInfo final
{
    QString displayName;
    QString executableName;
    QString path;

    [[nodiscard]] bool available() const noexcept
    {
        return !path.isEmpty();
    }
};

struct TeXEnvironmentSnapshot final
{
    TeXDistribution distribution = TeXDistribution::Unknown;
    QString customToolchainPath;

    // Required/primary direct-build tools.
    ToolInfo pdflatex;
    ToolInfo lualatex;
    ToolInfo xelatex;
    ToolInfo bibtex;
    ToolInfo biber;
    ToolInfo synctex;

    // Optional tools. Normal compilation does not depend on these.
    ToolInfo latexmk;
    ToolInfo perl;

    // Distribution evidence.
    ToolInfo initexmf;
    ToolInfo miktex;
    ToolInfo kpsewhich;
    ToolInfo tlmgr;
};

class IEnvironmentService
{
public:
    virtual ~IEnvironmentService() = default;

    virtual void setCustomToolchainPath(
        const QString& directory) = 0;

    [[nodiscard]] virtual QString customToolchainPath() const = 0;

    virtual void refresh() = 0;

    [[nodiscard]] virtual TeXEnvironmentSnapshot snapshot() const = 0;

    [[nodiscard]] virtual bool canBuildWith(
        TeXEngine engine) const = 0;

    [[nodiscard]] virtual QString engineExecutablePath(
        TeXEngine engine) const = 0;
};
