#include "app/ApplicationPaths.h"
#include "build/BuildService.h"
#include "infrastructure/EnvironmentService.h"
#include "infrastructure/Logging.h"
#include "infrastructure/QtDocumentFileService.h"
#include "infrastructure/QtProcessSupervisor.h"
#include "infrastructure/QtSettingsService.h"
#include "pdf/QtPdfBackend.h"
#include "preview/PreviewService.h"
#include "project/ProjectService.h"
#include "recovery/RecoveryService.h"
#include "synctex/SyncTeXService.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QTimer>

#include <cstdio>

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);

    QCoreApplication::setOrganizationName("NativeTeXEditor");
    QCoreApplication::setApplicationName("NativeTeXEditor");
    QCoreApplication::setApplicationVersion("0.4.0");

    const ApplicationPaths paths =
        ApplicationPaths::create();

    QString directoryError;
    if (!paths.ensureDirectories(&directoryError)) {
        std::fprintf(
            stderr,
            "NativeTeXEditor startup failed: %s\n",
            directoryError.toUtf8().constData());
        return 1;
    }

    if (!Logging::initialize(paths.logFilePath)) {
        std::fprintf(
            stderr,
            "NativeTeXEditor warning: unable to open log file: %s\n",
            paths.logFilePath.toUtf8().constData());
    }

    qInfo() << "NativeTeXEditor starting";
    qInfo() << "Application data:" << paths.dataDirectory;
    qInfo() << "Cache:" << paths.cacheDirectory;
    qInfo() << "Recovery:" << paths.recoveryDirectory;
    qInfo() << "Log file:" << paths.logFilePath;
    qInfo() << "Settings file:" << paths.settingsFilePath;

    QtSettingsService settingsService(
        paths.settingsFilePath);
    QtDocumentFileService documentFileService;
    ProjectService projectService;
    RecoveryService recoveryService(
        paths.recoveryDirectory);

    EnvironmentService environmentService;
    QtProcessSupervisor processSupervisor;
    BuildService buildService(
        environmentService,
        processSupervisor);
    QtProcessSupervisor syncProcessSupervisor;
    SyncTeXService syncTeXService(syncProcessSupervisor);
    QtPdfBackend pdfBackend;
    PreviewService previewService(
        paths.cacheDirectory,
        pdfBackend);

    MainWindow window(
        settingsService,
        documentFileService,
        projectService,
        recoveryService,
        environmentService,
        buildService,
        previewService,
        syncTeXService);

    window.show();

    QTimer::singleShot(
        0,
        &window,
        &MainWindow::processStartupRecovery);

    const int exitCode =
        application.exec();

    qInfo()
        << "NativeTeXEditor exiting with code"
        << exitCode;

    Logging::shutdown();
    return exitCode;
}
