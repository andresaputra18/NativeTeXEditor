#pragma once

#include "services/ISyncTeXService.h"

#include <QElapsedTimer>
#include <QStringList>

#include <memory>

class IProcessSupervisor;
struct ProcessResult;

class SyncTeXService final : public ISyncTeXService
{
public:
    explicit SyncTeXService(IProcessSupervisor& processSupervisor);
    ~SyncTeXService() override;

    [[nodiscard]] bool forward(
        const SyncTeXRequestContext& context,
        const SourcePosition& source,
        ResultCallback callback) override;
    [[nodiscard]] bool inverse(
        const SyncTeXRequestContext& context,
        const PdfPosition& pdf,
        ResultCallback callback) override;
    void cancel() override;
    [[nodiscard]] bool isRunning() const noexcept override;

private:
    enum class Direction
    {
        Forward,
        Inverse
    };

    [[nodiscard]] bool start(
        Direction direction,
        const SyncTeXRequestContext& context,
        const QStringList& arguments,
        ResultCallback callback);
    void finish(const ProcessResult& processResult);
    [[nodiscard]] SyncTeXResult parseForward(
        const ProcessResult& processResult) const;
    [[nodiscard]] SyncTeXResult parseInverse(
        const ProcessResult& processResult) const;
    [[nodiscard]] static QString valueAfterPrefix(
        const QString& line,
        const QString& prefix);
    [[nodiscard]] static QString directionName(Direction direction);

    IProcessSupervisor& processSupervisor_;
    Direction activeDirection_ = Direction::Forward;
    QString activeGenerationId_;
    ResultCallback activeCallback_;
    QElapsedTimer elapsedTimer_;
    std::shared_ptr<int> lifetimeToken_ = std::make_shared<int>(0);
};
