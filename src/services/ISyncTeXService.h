#pragma once

#include "synctex/SyncTeXTypes.h"

#include <functional>

class ISyncTeXService
{
public:
    using ResultCallback =
        std::function<void(const SyncTeXResult&)>;

    virtual ~ISyncTeXService() = default;

    [[nodiscard]] virtual bool forward(
        const SyncTeXRequestContext& context,
        const SourcePosition& source,
        ResultCallback callback) = 0;
    [[nodiscard]] virtual bool inverse(
        const SyncTeXRequestContext& context,
        const PdfPosition& pdf,
        ResultCallback callback) = 0;
    virtual void cancel() = 0;
    [[nodiscard]] virtual bool isRunning() const noexcept = 0;
};
