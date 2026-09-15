#pragma once

#include "build/BuildTypes.h"
#include "document/DocumentSession.h"
#include "services/IRecoveryService.h"
#include "synctex/SyncTeXTypes.h"
#include "ui/RecoveryComparisonDialog.h"

#include <QByteArray>
#include <QMainWindow>
#include <QStringList>

#include <optional>

class QAction;
class QActionGroup;
class QCloseEvent;
class QDockWidget;
class QFileSystemModel;
class QLabel;
class QMenu;
class QModelIndex;
class QPlainTextEdit;
class QSplitter;
class QStackedWidget;
class QTimer;
class QTreeView;
class QWidget;

class IBuildService;
class IDocumentFileService;
class IEditorBackend;
class IEnvironmentService;
class IProjectService;
class IPreviewService;
class ISettingsService;
class ISyncTeXService;

class MainWindow final : public QMainWindow
{
public:
    explicit MainWindow(
        ISettingsService& settingsService,
        IDocumentFileService& documentFileService,
        IProjectService& projectService,
        IRecoveryService& recoveryService,
        IEnvironmentService& environmentService,
        IBuildService& buildService,
        IPreviewService& previewService,
        ISyncTeXService& syncTeXService,
        QWidget* parent = nullptr);

    void processStartupRecovery();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    static constexpr int MaximumRecentFiles = 10;
    static constexpr int MaximumRecentProjects = 10;
    static constexpr int RecoveryIntervalMilliseconds = 10000;

    void createWorkspace();
    QWidget* createProjectPanel(QWidget* parent);
    void createMenus();
    void createStatusBar();

    void configureRecovery();
    void persistRecoverySnapshotIfNeeded();
    void clearRecoverySnapshot();
    void restoreRecoverySnapshot(const RecoverySnapshot& snapshot);
    [[nodiscard]] RecoveryComparisonDialog::Decision
        showRecoveryComparison(const RecoverySnapshot& snapshot);

    void configureBuildOutput();
    void startBuild();
    void stopBuild();
    void showSystemCheck();
    void showBuildOutput();
    void appendBuildOutput(const QString& text);
    void handleBuildFinished(const BuildResult& result);
    void syncSourceToPdf();
    void handleInverseSyncRequest(const PdfPosition& position);
    void handleForwardSyncResult(
        const QString& generationId,
        const SyncTeXResult& result);
    void handleInverseSyncResult(
        const QString& generationId,
        const SyncTeXResult& result);
    [[nodiscard]] std::optional<SyncTeXRequestContext>
        syncTeXContext(QString& errorMessage) const;
    void refreshPreviewTarget();
    void updateBuildActions();
    void setBuildEngine(TeXEngine engine);
    void setBuildTimeoutMilliseconds(int timeoutMilliseconds);
    void chooseCustomToolchainPath();
    void clearCustomToolchainPath();
    [[nodiscard]] QString resolvedBuildTarget() const;
    [[nodiscard]] bool saveRelevantDirtyDocumentBeforeBuild();

    void newDocument();
    void openDocument();
    void openDocumentPath(const QString& filePath);
    [[nodiscard]] bool saveDocument();
    [[nodiscard]] bool saveDocumentAs();
    [[nodiscard]] bool saveDocumentToPath(const QString& filePath);
    void closeDocument();
    [[nodiscard]] bool maybeSaveCurrentDocument();

    void updateWindowTitle();
    void loadRecentFiles();
    void saveRecentFiles();
    void addRecentFile(const QString& filePath);
    void removeRecentFile(const QString& filePath);
    void rebuildRecentFilesMenu();

    void openProject();
    void openProjectPath(const QString& rootPath, bool addToRecent = true);
    void closeProject();
    void restoreLastProject();
    void restoreProjectMainDocument();
    void setCurrentDocumentAsMain();
    void clearMainDocument();
    void updateProjectPanel();
    void updateProjectActions();
    void handleProjectTreeDoubleClick(const QModelIndex& index);

    void loadRecentProjects();
    void saveRecentProjects();
    void addRecentProject(const QString& rootPath);
    void removeRecentProject(const QString& rootPath);
    void rebuildRecentProjectsMenu();

    void resetWorkspaceLayout();
    void restoreWindowState();
    void saveWindowState();

    ISettingsService& settingsService_;
    IDocumentFileService& documentFileService_;
    IProjectService& projectService_;
    IRecoveryService& recoveryService_;
    IEnvironmentService& environmentService_;
    IBuildService& buildService_;
    IPreviewService& previewService_;
    ISyncTeXService& syncTeXService_;
    DocumentSession documentSession_;

    QTimer* recoveryTimer_ = nullptr;
    QByteArray lastRecoveryDigest_;
    bool recoveryManagementActive_ = false;
    bool recoveredBaselineDirty_ = false;

    QSplitter* workspaceSplitter_ = nullptr;
    QWidget* projectPanel_ = nullptr;
    QWidget* previewPanel_ = nullptr;
    IEditorBackend* editorBackend_ = nullptr;

    QFileSystemModel* projectFileModel_ = nullptr;
    QTreeView* projectTreeView_ = nullptr;
    QStackedWidget* projectStack_ = nullptr;
    QWidget* projectEmptyPage_ = nullptr;
    QWidget* projectTreePage_ = nullptr;
    QLabel* projectNameLabel_ = nullptr;
    QLabel* projectPathLabel_ = nullptr;
    QLabel* projectMainDocumentLabel_ = nullptr;

    QMenu* recentFilesMenu_ = nullptr;
    QStringList recentFiles_;

    QMenu* recentProjectsMenu_ = nullptr;
    QStringList recentProjects_;
    QAction* closeProjectAction_ = nullptr;
    QAction* setMainDocumentAction_ = nullptr;
    QAction* clearMainDocumentAction_ = nullptr;

    QDockWidget* buildOutputDock_ = nullptr;
    QPlainTextEdit* buildOutputEdit_ = nullptr;

    QAction* buildAction_ = nullptr;
    QAction* stopBuildAction_ = nullptr;
    QAction* syncSourceToPdfAction_ = nullptr;

    QActionGroup* buildEngineActionGroup_ = nullptr;
    QAction* pdfLatexAction_ = nullptr;
    QAction* luaLatexAction_ = nullptr;
    QAction* xeLatexAction_ = nullptr;

    QActionGroup* buildTimeoutActionGroup_ = nullptr;
    QAction* timeout30SecondsAction_ = nullptr;
    QAction* timeout2MinutesAction_ = nullptr;
    QAction* timeout5MinutesAction_ = nullptr;
    QAction* timeout10MinutesAction_ = nullptr;
    QAction* timeoutDisabledAction_ = nullptr;

    QAction* chooseToolchainPathAction_ = nullptr;
    QAction* clearToolchainPathAction_ = nullptr;

    TeXEngine buildEngine_ = TeXEngine::PdfLaTeX;
    int buildTimeoutMilliseconds_ = 10 * 60 * 1000;
    bool closeAfterBuildStops_ = false;

    QAction* projectPanelAction_ = nullptr;
    QAction* previewPanelAction_ = nullptr;
    QAction* statusBarAction_ = nullptr;
};
