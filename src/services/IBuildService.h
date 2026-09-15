#pragma once

#include "build/BuildTypes.h"

#include <QString>

#include <functional>

class IBuildService
{
public:
    using OutputCallback =
        std::function<void(const QString&)>;
    using FinishedCallback =
        std::function<void(const BuildResult&)>;

    virtual ~IBuildService() = default;

    [[nodiscard]] virtual bool startBuild(
        const BuildRequest& request,
        OutputCallback outputCallback,
        FinishedCallback finishedCallback) = 0;

    virtual void cancelBuild() = 0;

    [[nodiscard]] virtual bool isBuilding() const noexcept = 0;
};
