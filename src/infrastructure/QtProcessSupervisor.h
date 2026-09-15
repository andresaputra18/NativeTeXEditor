#pragma once

#include "services/IProcessSupervisor.h"

#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QTimer>

class QtProcessSupervisor final
    : public QObject
    , public IProcessSupervisor
{
public:
    explicit QtProcessSupervisor(QObject* parent = nullptr);
    ~QtProcessSupervisor() override;

    [[nodiscard]] bool start(
        const ProcessRequest& request,
        OutputCallback outputCallback,
        FinishedCallback finishedCallback) override;

    void cancel() override;

    [[nodiscard]] bool isRunning() const noexcept override;

private:
    void drainOutput();
    void terminateProcessTree();
    void handleTimeout();

    void finish(
        int exitCode,
        bool crashed,
        const QString& errorMessage = {});

    QProcess process_;
    QTimer timeoutTimer_;
    QElapsedTimer elapsedTimer_;

    OutputCallback outputCallback_;
    FinishedCallback finishedCallback_;

    QString collectedOutput_;

    bool active_ = false;
    bool processStarted_ = false;
    bool cancelled_ = false;
    bool timedOut_ = false;
};
