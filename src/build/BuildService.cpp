#include "build/BuildService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <utility>

BuildService::BuildService(
    IEnvironmentService& environmentService,
    IProcessSupervisor& processSupervisor)
    : environmentService_(environmentService)
    , processSupervisor_(processSupervisor)
{
}

QByteArray BuildService::readBytes(
    const QString& filePath)
{
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly))
        return {};

    return file.readAll();
}

QStringList BuildService::texArguments(
    const QString& fileName)
{
    return QStringList{
        "-interaction=nonstopmode",
        "-file-line-error",
        "-synctex=1",
        fileName
    };
}

bool BuildService::outputNeedsAnotherPass(
    const QString& output)
{
    static const QStringList indicators{
        "Rerun to get cross-references right",
        "Label(s) may have changed",
        "Please rerun LaTeX",
        "Please (re)run LaTeX",
        "Rerun to get outlines right",
        "rerunfilecheck Warning"
    };

    for (const QString& indicator : indicators) {
        if (output.contains(
                indicator,
                Qt::CaseInsensitive)) {
            return true;
        }
    }

    return false;
}

BuildService::BibliographyTool
BuildService::bibliographyToolNeeded(
    const QString& firstPassOutput) const
{
    const QByteArray currentBcf =
        readBytes(bcfPath_);

    if (!currentBcf.isEmpty()) {
        const bool bblMissing =
            !QFileInfo::exists(bblPath_);

        const bool bcfChanged =
            currentBcf != bcfBeforeBuild_;

        const bool outputRequestsBiber =
            firstPassOutput.contains(
                "Biber",
                Qt::CaseInsensitive);

        if (bblMissing
            || bcfChanged
            || outputRequestsBiber) {
            return BibliographyTool::Biber;
        }
    }

    const QByteArray currentAux =
        readBytes(auxPath_);

    if (currentAux.contains("\\bibdata{")) {
        const bool bblMissing =
            !QFileInfo::exists(bblPath_);

        const bool auxChanged =
            currentAux != auxBeforeBuild_;

        if (bblMissing || auxChanged)
            return BibliographyTool::BibTeX;
    }

    return BibliographyTool::None;
}

int BuildService::remainingTimeoutMilliseconds() const
{
    if (request_.timeoutMilliseconds <= 0)
        return 0;

    const qint64 remaining =
        static_cast<qint64>(
            request_.timeoutMilliseconds)
        - elapsedTimer_.elapsed();

    if (remaining <= 0)
        return -1;

    return static_cast<int>(remaining);
}

bool BuildService::startBuild(
    const BuildRequest& request,
    OutputCallback outputCallback,
    FinishedCallback finishedCallback)
{
    if (active_
        || processSupervisor_.isRunning()) {
        return false;
    }

    const QFileInfo targetInfo(request.targetPath);

    if (!targetInfo.exists()
        || !targetInfo.isFile()) {
        BuildResult result;
        result.targetPath = request.targetPath;
        result.errorMessage =
            QString(
                "Build target does not exist:\n%1")
                .arg(request.targetPath);

        if (finishedCallback)
            finishedCallback(result);

        return true;
    }

    environmentService_.refresh();
    environment_ =
        environmentService_.snapshot();

    if (!environmentService_.canBuildWith(
            request.engine)) {
        BuildResult result;
        result.targetPath = request.targetPath;
        result.errorMessage =
            QString(
                "%1 was not found in the configured "
                "toolchain path or system PATH.")
                .arg(
                    texEngineDisplayName(
                        request.engine));

        if (finishedCallback)
            finishedCallback(result);

        return true;
    }

    request_ = request;

    outputCallback_ =
        std::move(outputCallback);
    finishedCallback_ =
        std::move(finishedCallback);

    targetPath_ =
        targetInfo.absoluteFilePath();
    workingDirectory_ =
        targetInfo.absolutePath();
    fileName_ =
        targetInfo.fileName();
    baseName_ =
        targetInfo.completeBaseName();

    pdfPath_ =
        QDir(workingDirectory_)
            .filePath(baseName_ + ".pdf");
    auxPath_ =
        QDir(workingDirectory_)
            .filePath(baseName_ + ".aux");
    bcfPath_ =
        QDir(workingDirectory_)
            .filePath(baseName_ + ".bcf");
    bblPath_ =
        QDir(workingDirectory_)
            .filePath(baseName_ + ".bbl");

    auxBeforeBuild_ =
        readBytes(auxPath_);
    bcfBeforeBuild_ =
        readBytes(bcfPath_);

    combinedOutput_.clear();

    texPassCount_ = 0;
    forcedPassesRemaining_ = 0;
    lastExitCode_ = -1;

    cancellationRequested_ = false;
    anyProcessStarted_ = false;
    bibliographyChecked_ = false;
    active_ = true;

    elapsedTimer_.restart();

    startTeXPass();
    return true;
}

void BuildService::startTeXPass()
{
    if (!active_)
        return;

    if (cancellationRequested_) {
        completeCancelled();
        return;
    }

    if (texPassCount_ >= MaximumTeXPasses) {
        appendOutput(
            "\nMaximum TeX pass count reached; "
            "using the latest successful result.\n");
        completeSuccess();
        return;
    }

    const int remaining =
        remainingTimeoutMilliseconds();

    if (remaining < 0) {
        completeTimedOut();
        return;
    }

    ++texPassCount_;

    appendOutput(
        QString(
            "\n--- %1 pass %2 ---\n")
            .arg(
                texEngineDisplayName(
                    request_.engine))
            .arg(texPassCount_));

    ProcessRequest processRequest;
    processRequest.program =
        environmentService_
            .engineExecutablePath(
                request_.engine);
    processRequest.arguments =
        texArguments(fileName_);
    processRequest.workingDirectory =
        workingDirectory_;
    processRequest.timeoutMilliseconds =
        remaining;

    const bool accepted =
        processSupervisor_.start(
            processRequest,
            [this](const QString& text) {
                appendOutput(text);
            },
            [this](const ProcessResult& result) {
                onTeXFinished(result);
            });

    if (!accepted) {
        completeFailure(
            -1,
            "Unable to start the TeX process.");
    }
}

void BuildService::onTeXFinished(
    const ProcessResult& result)
{
    if (!active_)
        return;

    anyProcessStarted_ =
        anyProcessStarted_ || result.started;
    lastExitCode_ = result.exitCode;

    if (result.cancelled
        || cancellationRequested_) {
        completeCancelled();
        return;
    }

    if (result.timedOut) {
        completeTimedOut();
        return;
    }

    if (result.crashed) {
        completeFailure(
            result.exitCode,
            result.errorMessage.isEmpty()
                ? QString(
                      "The TeX process crashed.")
                : result.errorMessage,
            true);
        return;
    }

    if (!result.started) {
        completeFailure(
            result.exitCode,
            result.errorMessage.isEmpty()
                ? QString(
                      "The TeX process failed to start.")
                : result.errorMessage);
        return;
    }

    if (result.exitCode != 0) {
        completeFailure(
            result.exitCode,
            result.errorMessage);
        return;
    }

    if (!bibliographyChecked_) {
        bibliographyChecked_ = true;

        const BibliographyTool tool =
            bibliographyToolNeeded(
                result.output);

        if (tool != BibliographyTool::None) {
            startBibliography(tool);
            return;
        }
    }

    if (forcedPassesRemaining_ > 0) {
        --forcedPassesRemaining_;

        if (forcedPassesRemaining_ > 0) {
            startTeXPass();
            return;
        }
    }

    if (outputNeedsAnotherPass(
            result.output)
        && texPassCount_ < MaximumTeXPasses) {
        startTeXPass();
        return;
    }

    completeSuccess();
}

void BuildService::startBibliography(
    BibliographyTool tool)
{
    if (!active_)
        return;

    if (cancellationRequested_) {
        completeCancelled();
        return;
    }

    const int remaining =
        remainingTimeoutMilliseconds();

    if (remaining < 0) {
        completeTimedOut();
        return;
    }

    QString program;
    QString displayName;

    if (tool == BibliographyTool::Biber) {
        program = environment_.biber.path;
        displayName = "Biber";
    } else {
        program = environment_.bibtex.path;
        displayName = "BibTeX";
    }

    if (program.isEmpty()) {
        completeFailure(
            -1,
            QString(
                "%1 is required by this document "
                "but was not found.")
                .arg(displayName));
        return;
    }

    appendOutput(
        QString(
            "\n--- %1 bibliography pass ---\n")
            .arg(displayName));

    ProcessRequest processRequest;
    processRequest.program = program;
    processRequest.arguments =
        QStringList{baseName_};
    processRequest.workingDirectory =
        workingDirectory_;
    processRequest.timeoutMilliseconds =
        remaining;

    const bool accepted =
        processSupervisor_.start(
            processRequest,
            [this](const QString& text) {
                appendOutput(text);
            },
            [this, tool](
                const ProcessResult& result) {
                onBibliographyFinished(
                    tool,
                    result);
            });

    if (!accepted) {
        completeFailure(
            -1,
            "Unable to start the bibliography process.");
    }
}

void BuildService::onBibliographyFinished(
    BibliographyTool tool,
    const ProcessResult& result)
{
    Q_UNUSED(tool);

    if (!active_)
        return;

    anyProcessStarted_ =
        anyProcessStarted_ || result.started;
    lastExitCode_ = result.exitCode;

    if (result.cancelled
        || cancellationRequested_) {
        completeCancelled();
        return;
    }

    if (result.timedOut) {
        completeTimedOut();
        return;
    }

    if (result.crashed) {
        completeFailure(
            result.exitCode,
            result.errorMessage.isEmpty()
                ? QString(
                      "The bibliography process crashed.")
                : result.errorMessage,
            true);
        return;
    }

    if (!result.started
        || result.exitCode != 0) {
        completeFailure(
            result.exitCode,
            result.errorMessage);
        return;
    }

    // Bibliography output must be consumed by TeX. Two passes cover
    // citation insertion and the subsequent reference stabilization.
    forcedPassesRemaining_ = 2;
    startTeXPass();
}

void BuildService::appendOutput(
    const QString& text)
{
    if (text.isEmpty())
        return;

    combinedOutput_.append(text);

    if (outputCallback_)
        outputCallback_(text);
}

void BuildService::completeSuccess()
{
    BuildResult result;
    result.started = anyProcessStarted_;
    result.success =
        anyProcessStarted_
        && QFileInfo::exists(pdfPath_);
    result.exitCode = lastExitCode_;
    result.elapsedMilliseconds =
        elapsedTimer_.elapsed();
    result.targetPath = targetPath_;
    result.pdfPath = pdfPath_;
    result.output = combinedOutput_;

    if (!result.success) {
        result.errorMessage =
            "The TeX process completed, but the "
            "expected PDF was not produced.";
    }

    complete(std::move(result));
}

void BuildService::completeFailure(
    int exitCode,
    const QString& errorMessage,
    bool crashed)
{
    BuildResult result;
    result.started = anyProcessStarted_;
    result.exitCode = exitCode;
    result.elapsedMilliseconds =
        elapsedTimer_.elapsed();
    result.targetPath = targetPath_;
    result.pdfPath = pdfPath_;
    result.output = combinedOutput_;
    result.errorMessage = errorMessage;
    result.crashed = crashed;

    complete(std::move(result));
}

void BuildService::completeCancelled()
{
    BuildResult result;
    result.started = anyProcessStarted_;
    result.cancelled = true;
    result.exitCode = lastExitCode_;
    result.elapsedMilliseconds =
        elapsedTimer_.elapsed();
    result.targetPath = targetPath_;
    result.pdfPath = pdfPath_;
    result.output = combinedOutput_;

    complete(std::move(result));
}

void BuildService::completeTimedOut()
{
    BuildResult result;
    result.started = anyProcessStarted_;
    result.timedOut = true;
    result.exitCode = lastExitCode_;
    result.elapsedMilliseconds =
        elapsedTimer_.elapsed();
    result.targetPath = targetPath_;
    result.pdfPath = pdfPath_;
    result.output = combinedOutput_;

    complete(std::move(result));
}

void BuildService::complete(
    BuildResult result)
{
    if (!active_)
        return;

    active_ = false;

    auto callback =
        std::move(finishedCallback_);

    outputCallback_ = {};
    finishedCallback_ = {};

    if (callback)
        callback(result);
}

void BuildService::cancelBuild()
{
    if (!active_)
        return;

    cancellationRequested_ = true;

    if (processSupervisor_.isRunning()) {
        processSupervisor_.cancel();
        return;
    }

    completeCancelled();
}

bool BuildService::isBuilding() const noexcept
{
    return active_;
}
