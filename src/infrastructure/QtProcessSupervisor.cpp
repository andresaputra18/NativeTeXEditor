#include "infrastructure/QtProcessSupervisor.h"

#include <QProcessEnvironment>

#include <utility>

QtProcessSupervisor::QtProcessSupervisor(QObject* parent)
    : QObject(parent)
{
    process_.setProcessChannelMode(
        QProcess::MergedChannels);

    timeoutTimer_.setSingleShot(true);

    connect(
        &timeoutTimer_,
        &QTimer::timeout,
        this,
        &QtProcessSupervisor::handleTimeout);

    connect(
        &process_,
        &QProcess::started,
        this,
        [this] {
            processStarted_ = true;
        });

    connect(
        &process_,
        &QProcess::readyReadStandardOutput,
        this,
        &QtProcessSupervisor::drainOutput);

    connect(
        &process_,
        &QProcess::finished,
        this,
        [this](
            int exitCode,
            QProcess::ExitStatus exitStatus) {
            if (!active_)
                return;

            drainOutput();

            finish(
                exitCode,
                exitStatus == QProcess::CrashExit);
        });

    connect(
        &process_,
        &QProcess::errorOccurred,
        this,
        [this](QProcess::ProcessError error) {
            if (!active_)
                return;

            if (error == QProcess::FailedToStart) {
                finish(
                    -1,
                    false,
                    process_.errorString());
            }
        });
}

QtProcessSupervisor::~QtProcessSupervisor()
{
    if (!active_)
        return;

    cancelled_ = true;
    timeoutTimer_.stop();
    terminateProcessTree();

    if (!process_.waitForFinished(1500)) {
        process_.kill();
        process_.waitForFinished(1000);
    }
}

bool QtProcessSupervisor::start(
    const ProcessRequest& request,
    OutputCallback outputCallback,
    FinishedCallback finishedCallback)
{
    if (active_)
        return false;

    outputCallback_ =
        std::move(outputCallback);
    finishedCallback_ =
        std::move(finishedCallback);

    collectedOutput_.clear();
    processStarted_ = false;
    cancelled_ = false;
    timedOut_ = false;
    active_ = true;

    elapsedTimer_.restart();

    process_.setProcessEnvironment(
        QProcessEnvironment::systemEnvironment());
    process_.setWorkingDirectory(
        request.workingDirectory);
    process_.setProgram(request.program);
    process_.setArguments(request.arguments);

    if (request.timeoutMilliseconds > 0) {
        timeoutTimer_.start(
            request.timeoutMilliseconds);
    }

    process_.start();
    return true;
}

void QtProcessSupervisor::cancel()
{
    if (!active_)
        return;

    cancelled_ = true;
    timeoutTimer_.stop();
    terminateProcessTree();
}

bool QtProcessSupervisor::isRunning() const noexcept
{
    return active_;
}

void QtProcessSupervisor::drainOutput()
{
    const QByteArray bytes =
        process_.readAllStandardOutput();

    if (bytes.isEmpty())
        return;

    const QString text =
        QString::fromLocal8Bit(bytes);

    collectedOutput_.append(text);

    if (outputCallback_)
        outputCallback_(text);
}

void QtProcessSupervisor::terminateProcessTree()
{
#ifdef Q_OS_WIN
    const qint64 pid = process_.processId();

    if (pid > 0) {
        QProcess::startDetached(
            "taskkill",
            QStringList{
                "/PID",
                QString::number(pid),
                "/T",
                "/F"
            });
    }
#endif

    if (process_.state() != QProcess::NotRunning) {
        process_.terminate();
    }

    QTimer::singleShot(
        1500,
        this,
        [this] {
            if (active_
                && process_.state()
                    != QProcess::NotRunning) {
                process_.kill();
            }
        });
}

void QtProcessSupervisor::handleTimeout()
{
    if (!active_)
        return;

    timedOut_ = true;
    terminateProcessTree();
}

void QtProcessSupervisor::finish(
    int exitCode,
    bool crashed,
    const QString& errorMessage)
{
    if (!active_)
        return;

    timeoutTimer_.stop();

    ProcessResult result;
    result.started = processStarted_;
    result.cancelled = cancelled_;
    result.timedOut = timedOut_;
    result.crashed = crashed;
    result.exitCode = exitCode;
    result.elapsedMilliseconds =
        elapsedTimer_.isValid()
            ? elapsedTimer_.elapsed()
            : 0;
    result.output = collectedOutput_;
    result.errorMessage = errorMessage;

    active_ = false;

    auto callback =
        std::move(finishedCallback_);

    outputCallback_ = {};
    finishedCallback_ = {};

    if (callback)
        callback(result);
}
