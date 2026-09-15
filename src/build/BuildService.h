#pragma once

#include "services/IBuildService.h"
#include "services/IEnvironmentService.h"
#include "services/IProcessSupervisor.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QString>

class BuildService final : public IBuildService
{
public:
    BuildService(
        IEnvironmentService& environmentService,
        IProcessSupervisor& processSupervisor);

    [[nodiscard]] bool startBuild(
        const BuildRequest& request,
        OutputCallback outputCallback,
        FinishedCallback finishedCallback) override;

    void cancelBuild() override;

    [[nodiscard]] bool isBuilding() const noexcept override;

private:
    static constexpr int MaximumTeXPasses = 5;

    enum class BibliographyTool
    {
        None,
        BibTeX,
        Biber
    };

    [[nodiscard]] static QByteArray readBytes(
        const QString& filePath);

    [[nodiscard]] static QStringList texArguments(
        const QString& fileName);

    [[nodiscard]] static bool outputNeedsAnotherPass(
        const QString& output);

    [[nodiscard]] BibliographyTool bibliographyToolNeeded(
        const QString& firstPassOutput) const;

    [[nodiscard]] int remainingTimeoutMilliseconds() const;

    void startTeXPass();
    void onTeXFinished(const ProcessResult& result);

    void startBibliography(BibliographyTool tool);
    void onBibliographyFinished(
        BibliographyTool tool,
        const ProcessResult& result);

    void appendOutput(const QString& text);

    void completeSuccess();
    void completeFailure(
        int exitCode,
        const QString& errorMessage,
        bool crashed = false);
    void completeCancelled();
    void completeTimedOut();

    void complete(BuildResult result);

    IEnvironmentService& environmentService_;
    IProcessSupervisor& processSupervisor_;

    BuildRequest request_;
    TeXEnvironmentSnapshot environment_;

    OutputCallback outputCallback_;
    FinishedCallback finishedCallback_;

    QElapsedTimer elapsedTimer_;

    QString targetPath_;
    QString workingDirectory_;
    QString fileName_;
    QString baseName_;
    QString pdfPath_;
    QString auxPath_;
    QString bcfPath_;
    QString bblPath_;

    QByteArray auxBeforeBuild_;
    QByteArray bcfBeforeBuild_;

    QString combinedOutput_;

    int texPassCount_ = 0;
    int forcedPassesRemaining_ = 0;
    int lastExitCode_ = -1;

    bool active_ = false;
    bool cancellationRequested_ = false;
    bool anyProcessStarted_ = false;
    bool bibliographyChecked_ = false;
};
