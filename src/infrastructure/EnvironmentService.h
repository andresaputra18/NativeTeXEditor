#pragma once

#include "services/IEnvironmentService.h"

class EnvironmentService final : public IEnvironmentService
{
public:
    EnvironmentService();

    void setCustomToolchainPath(
        const QString& directory) override;

    [[nodiscard]] QString customToolchainPath() const override;

    void refresh() override;

    [[nodiscard]] TeXEnvironmentSnapshot snapshot() const override;

    [[nodiscard]] bool canBuildWith(
        TeXEngine engine) const override;

    [[nodiscard]] QString engineExecutablePath(
        TeXEngine engine) const override;

private:
    [[nodiscard]] ToolInfo findTool(
        const QString& displayName,
        const QString& executableName) const;

    [[nodiscard]] TeXDistribution detectDistribution() const;

    QString customToolchainPath_;
    TeXEnvironmentSnapshot snapshot_;
};
