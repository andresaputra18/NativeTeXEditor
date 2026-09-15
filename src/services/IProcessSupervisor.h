#pragma once

#include <QtGlobal>
#include <QString>
#include <QStringList>

#include <functional>

struct ProcessRequest final
{
    QString program;
    QStringList arguments;
    QString workingDirectory;

    // Per-process timeout. Zero disables timeout.
    int timeoutMilliseconds = 0;
};

struct ProcessResult final
{
    bool started = false;
    bool cancelled = false;
    bool timedOut = false;
    bool crashed = false;

    int exitCode = -1;
    qint64 elapsedMilliseconds = 0;

    QString output;
    QString errorMessage;
};

class IProcessSupervisor
{
public:
    using OutputCallback =
        std::function<void(const QString&)>;
    using FinishedCallback =
        std::function<void(const ProcessResult&)>;

    virtual ~IProcessSupervisor() = default;

    [[nodiscard]] virtual bool start(
        const ProcessRequest& request,
        OutputCallback outputCallback,
        FinishedCallback finishedCallback) = 0;

    virtual void cancel() = 0;

    [[nodiscard]] virtual bool isRunning() const noexcept = 0;
};
