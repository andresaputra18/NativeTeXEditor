#include "ui/MainWindow.h"

#include "editor/IEditorBackend.h"
#include "services/IBuildService.h"
#include "services/IEnvironmentService.h"
#include "editor/ScintillaEditorWidget.h"
#include "services/IDocumentFileService.h"
#include "services/IProjectService.h"
#include "services/IPreviewService.h"
#include "services/IRecoveryService.h"
#include "services/ISettingsService.h"
#include "services/ISyncTeXService.h"
#include "ui/RecoveryComparisonDialog.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFont>
#include <QFrame>
#include <QKeySequence>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QModelIndex>
#include <QPushButton>
#include <QPointer>
#include <QSet>
#include <QSizePolicy>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTextCursor>
#include <QTimer>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWidget>

namespace
{
QString documentFilter()
{
    return
        "TeX and bibliography files (*.tex *.bib *.sty *.cls *.ltx);;"
        "TeX files (*.tex *.ltx);;"
        "Bibliography files (*.bib);;"
        "TeX support files (*.sty *.cls);;"
        "Text files (*.txt);;"
        "All files (*.*)";
}

bool isEditableProjectFile(const QString& filePath)
{
    static const QSet<QString> editableExtensions = {
        "tex", "ltx", "bib", "sty", "cls",
        "txt", "md", "cfg", "def", "bst",
        "bbx", "cbx", "dtx", "ins", "tikz"
    };

    return editableExtensions.contains(
        QFileInfo(filePath).suffix().toLower());
}

bool isMainDocumentCandidate(const QString& filePath)
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    return suffix == "tex" || suffix == "ltx";
}

QString projectSettingsId(const QString& rootPath)
{
    const QByteArray digest = QCryptographicHash::hash(
        QDir::cleanPath(rootPath).toUtf8(),
        QCryptographicHash::Sha256);

    return QString::fromLatin1(digest.toHex());
}

QString mainDocumentSettingsKey(const QString& rootPath)
{
    return QString("projects/mainDocuments/%1")
        .arg(projectSettingsId(rootPath));
}

QString formatBuildElapsed(qint64 milliseconds)
{
    if (milliseconds < 1000)
        return QString("%1 ms").arg(milliseconds);

    const double seconds =
        static_cast<double>(milliseconds) / 1000.0;

    if (seconds < 60.0) {
        return QString("%1 s")
            .arg(seconds, 0, 'f', 2);
    }

    const qint64 totalSeconds =
        milliseconds / 1000;

    return QString("%1 min %2 s")
        .arg(totalSeconds / 60)
        .arg(totalSeconds % 60);
}

QString buildTimeoutLabel(int milliseconds)
{
    if (milliseconds <= 0)
        return "Disabled";

    if (milliseconds < 60 * 1000) {
        return QString("%1 seconds")
            .arg(milliseconds / 1000);
    }

    return QString("%1 minutes")
        .arg(milliseconds / (60 * 1000));
}

QByteArray recoveryDigest(
    const QString& originalFilePath,
    const QString& text)
{
    QByteArray payload = originalFilePath.toUtf8();
    payload.append('\0');
    payload.append(text.toUtf8());

    return QCryptographicHash::hash(
        payload,
        QCryptographicHash::Sha256);
}
}

MainWindow::MainWindow(
    ISettingsService& settingsService,
    IDocumentFileService& documentFileService,
    IProjectService& projectService,
    IRecoveryService& recoveryService,
    IEnvironmentService& environmentService,
    IBuildService& buildService,
    IPreviewService& previewService,
    ISyncTeXService& syncTeXService,
    QWidget* parent)
    : QMainWindow(parent)
    , settingsService_(settingsService)
    , documentFileService_(documentFileService)
    , projectService_(projectService)
    , recoveryService_(recoveryService)
    , environmentService_(environmentService)
    , buildService_(buildService)
    , previewService_(previewService)
    , syncTeXService_(syncTeXService)
{
    resize(1200, 720);
    setMinimumSize(900, 560);

    createWorkspace();

    previewService_.setInverseSyncCallback(
        [this](const PdfPosition& position) {
            handleInverseSyncRequest(position);
        });

    editorBackend_->setModifiedStateCallback(
        [this](bool modified) {
            const bool logicalModified =
                modified || recoveredBaselineDirty_;

            documentSession_.setDirty(logicalModified);

            if (recoveryManagementActive_) {
                if (logicalModified)
                    persistRecoverySnapshotIfNeeded();
                else
                    clearRecoverySnapshot();
            }

            updateWindowTitle();
        });

    environmentService_.setCustomToolchainPath(
        settingsService_.value(
            "build/customToolchainPath").toString());

    configureRecovery();
    configureBuildOutput();
    createMenus();
    createStatusBar();
    loadRecentFiles();
    loadRecentProjects();
    restoreWindowState();
    newDocument();
    restoreLastProject();
}

QWidget* MainWindow::createProjectPanel(QWidget* parent)
{
    auto* panel = new QFrame(parent);
    panel->setFrameShape(QFrame::StyledPanel);
    panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(10, 8, 10, 10);
    layout->setSpacing(6);

    auto* titleLabel = new QLabel("Project", panel);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    projectNameLabel_ = new QLabel(panel);
    QFont projectNameFont = projectNameLabel_->font();
    projectNameFont.setBold(true);
    projectNameLabel_->setFont(projectNameFont);

    projectPathLabel_ = new QLabel(panel);
    projectPathLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    projectPathLabel_->setWordWrap(true);

    projectMainDocumentLabel_ = new QLabel(panel);
    projectMainDocumentLabel_->setWordWrap(true);

    projectStack_ = new QStackedWidget(panel);

    projectEmptyPage_ = new QWidget(projectStack_);
    auto* emptyLayout = new QVBoxLayout(projectEmptyPage_);
    emptyLayout->setContentsMargins(8, 8, 8, 8);

    auto* emptyLabel = new QLabel(
        "No project open\n\nProject → Open Project Folder...",
        projectEmptyPage_);
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLabel->setWordWrap(true);
    emptyLayout->addWidget(emptyLabel, 1);

    projectTreePage_ = new QWidget(projectStack_);
    auto* treeLayout = new QVBoxLayout(projectTreePage_);
    treeLayout->setContentsMargins(0, 0, 0, 0);

    projectFileModel_ = new QFileSystemModel(projectTreePage_);
    projectFileModel_->setReadOnly(true);
    projectFileModel_->setFilter(
        QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);

    projectTreeView_ = new QTreeView(projectTreePage_);
    projectTreeView_->setModel(projectFileModel_);
    projectTreeView_->setHeaderHidden(true);
    projectTreeView_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    projectTreeView_->setSelectionMode(QAbstractItemView::SingleSelection);
    projectTreeView_->setUniformRowHeights(true);
    projectTreeView_->setAnimated(false);
    projectTreeView_->setExpandsOnDoubleClick(true);

    for (int column = 1; column < projectFileModel_->columnCount(); ++column)
        projectTreeView_->hideColumn(column);

    connect(
        projectTreeView_,
        &QTreeView::doubleClicked,
        this,
        &MainWindow::handleProjectTreeDoubleClick);

    treeLayout->addWidget(projectTreeView_, 1);

    projectStack_->addWidget(projectEmptyPage_);
    projectStack_->addWidget(projectTreePage_);

    layout->addWidget(titleLabel);
    layout->addWidget(projectNameLabel_);
    layout->addWidget(projectPathLabel_);
    layout->addWidget(projectMainDocumentLabel_);
    layout->addWidget(projectStack_, 1);

    projectNameLabel_->hide();
    projectPathLabel_->hide();
    projectMainDocumentLabel_->hide();
    projectStack_->setCurrentWidget(projectEmptyPage_);

    return panel;
}

void MainWindow::createWorkspace()
{
    workspaceSplitter_ = new QSplitter(Qt::Horizontal, this);
    workspaceSplitter_->setChildrenCollapsible(false);
    workspaceSplitter_->setHandleWidth(4);

    projectPanel_ = createProjectPanel(workspaceSplitter_);

    auto* scintillaEditor = new ScintillaEditorWidget(workspaceSplitter_);
    editorBackend_ = scintillaEditor;

    previewPanel_ = previewService_.widget();
    previewPanel_->setParent(workspaceSplitter_);

    projectPanel_->setMinimumWidth(170);
    editorBackend_->widget()->setMinimumWidth(300);
    previewPanel_->setMinimumWidth(220);

    workspaceSplitter_->addWidget(projectPanel_);
    workspaceSplitter_->addWidget(editorBackend_->widget());
    workspaceSplitter_->addWidget(previewPanel_);
    workspaceSplitter_->setStretchFactor(0, 2);
    workspaceSplitter_->setStretchFactor(1, 5);
    workspaceSplitter_->setStretchFactor(2, 3);

    setCentralWidget(workspaceSplitter_);
}

void MainWindow::createMenus()
{
    auto* fileMenu = menuBar()->addMenu("&File");

    auto* newAction = fileMenu->addAction("&New");
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &MainWindow::newDocument);

    auto* openAction = fileMenu->addAction("&Open...");
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::openDocument);

    recentFilesMenu_ = fileMenu->addMenu("Open &Recent");

    fileMenu->addSeparator();

    auto* saveAction = fileMenu->addAction("&Save");
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, [this] {
        static_cast<void>(saveDocument());
    });

    auto* saveAsAction = fileMenu->addAction("Save &As...");
    saveAsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    connect(saveAsAction, &QAction::triggered, this, [this] {
        static_cast<void>(saveDocumentAs());
    });

    fileMenu->addSeparator();

    auto* closeAction = fileMenu->addAction("&Close Document");
    closeAction->setShortcut(QKeySequence::Close);
    connect(closeAction, &QAction::triggered, this, &MainWindow::closeDocument);

    fileMenu->addSeparator();

    auto* exitAction = fileMenu->addAction("E&xit");
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    auto* editMenu = menuBar()->addMenu("&Edit");

    auto* undoAction = editMenu->addAction("&Undo");
    undoAction->setShortcut(QKeySequence::Undo);
    connect(undoAction, &QAction::triggered, this, [this] { editorBackend_->undo(); });

    auto* redoAction = editMenu->addAction("&Redo");
    redoAction->setShortcut(QKeySequence::Redo);
    connect(redoAction, &QAction::triggered, this, [this] { editorBackend_->redo(); });

    editMenu->addSeparator();

    auto* cutAction = editMenu->addAction("Cu&t");
    cutAction->setShortcut(QKeySequence::Cut);
    connect(cutAction, &QAction::triggered, this, [this] { editorBackend_->cut(); });

    auto* copyAction = editMenu->addAction("&Copy");
    copyAction->setShortcut(QKeySequence::Copy);
    connect(copyAction, &QAction::triggered, this, [this] { editorBackend_->copy(); });

    auto* pasteAction = editMenu->addAction("&Paste");
    pasteAction->setShortcut(QKeySequence::Paste);
    connect(pasteAction, &QAction::triggered, this, [this] { editorBackend_->paste(); });

    editMenu->addSeparator();

    auto* selectAllAction = editMenu->addAction("Select &All");
    selectAllAction->setShortcut(QKeySequence::SelectAll);
    connect(selectAllAction, &QAction::triggered, this, [this] { editorBackend_->selectAll(); });

    auto* projectMenu = menuBar()->addMenu("&Project");

    auto* openProjectAction = projectMenu->addAction("&Open Project Folder...");
    openProjectAction->setShortcut(
        QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_O));
    connect(
        openProjectAction,
        &QAction::triggered,
        this,
        &MainWindow::openProject);

    recentProjectsMenu_ = projectMenu->addMenu("Open Recent &Project");

    projectMenu->addSeparator();

    closeProjectAction_ = projectMenu->addAction("&Close Project");
    connect(
        closeProjectAction_,
        &QAction::triggered,
        this,
        &MainWindow::closeProject);

    projectMenu->addSeparator();

    setMainDocumentAction_ =
        projectMenu->addAction("Set Current Document as &Main");
    connect(
        setMainDocumentAction_,
        &QAction::triggered,
        this,
        &MainWindow::setCurrentDocumentAsMain);

    clearMainDocumentAction_ =
        projectMenu->addAction("C&lear Main Document");
    connect(
        clearMainDocumentAction_,
        &QAction::triggered,
        this,
        &MainWindow::clearMainDocument);

    auto* buildMenu = menuBar()->addMenu("&Build");

    buildAction_ =
        buildMenu->addAction("&Build Document");
    buildAction_->setShortcut(
        QKeySequence(Qt::CTRL | Qt::Key_B));
    connect(
        buildAction_,
        &QAction::triggered,
        this,
        &MainWindow::startBuild);

    stopBuildAction_ =
        buildMenu->addAction("&Stop Build");
    connect(
        stopBuildAction_,
        &QAction::triggered,
        this,
        &MainWindow::stopBuild);

    syncSourceToPdfAction_ =
        buildMenu->addAction("Sync Source to &PDF");
    syncSourceToPdfAction_->setShortcut(
        QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_J));
    connect(
        syncSourceToPdfAction_,
        &QAction::triggered,
        this,
        &MainWindow::syncSourceToPdf);

    buildMenu->addSeparator();

    auto* engineMenu =
        buildMenu->addMenu("TeX &Engine");

    buildEngineActionGroup_ =
        new QActionGroup(this);
    buildEngineActionGroup_->setExclusive(true);

    pdfLatexAction_ =
        engineMenu->addAction("&pdfLaTeX");
    luaLatexAction_ =
        engineMenu->addAction("&LuaLaTeX");
    xeLatexAction_ =
        engineMenu->addAction("&XeLaTeX");

    for (QAction* action :
         {pdfLatexAction_,
          luaLatexAction_,
          xeLatexAction_}) {
        action->setCheckable(true);
        buildEngineActionGroup_->addAction(action);
    }

    connect(
        pdfLatexAction_,
        &QAction::triggered,
        this,
        [this] {
            setBuildEngine(
                TeXEngine::PdfLaTeX);
        });

    connect(
        luaLatexAction_,
        &QAction::triggered,
        this,
        [this] {
            setBuildEngine(
                TeXEngine::LuaLaTeX);
        });

    connect(
        xeLatexAction_,
        &QAction::triggered,
        this,
        [this] {
            setBuildEngine(
                TeXEngine::XeLaTeX);
        });

    auto* timeoutMenu =
        buildMenu->addMenu("Build &Timeout");

    buildTimeoutActionGroup_ =
        new QActionGroup(this);
    buildTimeoutActionGroup_->setExclusive(true);

    timeout30SecondsAction_ =
        timeoutMenu->addAction("30 seconds");
    timeout2MinutesAction_ =
        timeoutMenu->addAction("2 minutes");
    timeout5MinutesAction_ =
        timeoutMenu->addAction("5 minutes");
    timeout10MinutesAction_ =
        timeoutMenu->addAction("10 minutes");
    timeoutDisabledAction_ =
        timeoutMenu->addAction("Disabled");

    for (QAction* action :
         {timeout30SecondsAction_,
          timeout2MinutesAction_,
          timeout5MinutesAction_,
          timeout10MinutesAction_,
          timeoutDisabledAction_}) {
        action->setCheckable(true);
        buildTimeoutActionGroup_->addAction(action);
    }

    connect(
        timeout30SecondsAction_,
        &QAction::triggered,
        this,
        [this] {
            setBuildTimeoutMilliseconds(
                30 * 1000);
        });

    connect(
        timeout2MinutesAction_,
        &QAction::triggered,
        this,
        [this] {
            setBuildTimeoutMilliseconds(
                2 * 60 * 1000);
        });

    connect(
        timeout5MinutesAction_,
        &QAction::triggered,
        this,
        [this] {
            setBuildTimeoutMilliseconds(
                5 * 60 * 1000);
        });

    connect(
        timeout10MinutesAction_,
        &QAction::triggered,
        this,
        [this] {
            setBuildTimeoutMilliseconds(
                10 * 60 * 1000);
        });

    connect(
        timeoutDisabledAction_,
        &QAction::triggered,
        this,
        [this] {
            setBuildTimeoutMilliseconds(0);
        });

    auto* toolchainMenu =
        buildMenu->addMenu("&Toolchain");

    chooseToolchainPathAction_ =
        toolchainMenu->addAction(
            "Choose TeX &Bin Folder...");
    connect(
        chooseToolchainPathAction_,
        &QAction::triggered,
        this,
        &MainWindow::chooseCustomToolchainPath);

    clearToolchainPathAction_ =
        toolchainMenu->addAction(
            "Use System &PATH");
    connect(
        clearToolchainPathAction_,
        &QAction::triggered,
        this,
        &MainWindow::clearCustomToolchainPath);

    buildMenu->addSeparator();

    auto* systemCheckAction =
        buildMenu->addAction("&System Check...");
    connect(
        systemCheckAction,
        &QAction::triggered,
        this,
        &MainWindow::showSystemCheck);

    auto* showOutputAction =
        buildMenu->addAction(
            "Show Build &Output");
    connect(
        showOutputAction,
        &QAction::triggered,
        this,
        &MainWindow::showBuildOutput);

    buildEngine_ =
        texEngineFromSettingValue(
            settingsService_.value(
                "build/engine",
                "pdflatex").toString());

    buildTimeoutMilliseconds_ =
        settingsService_.value(
            "build/timeoutMilliseconds",
            10 * 60 * 1000).toInt();

    pdfLatexAction_->setChecked(
        buildEngine_ == TeXEngine::PdfLaTeX);
    luaLatexAction_->setChecked(
        buildEngine_ == TeXEngine::LuaLaTeX);
    xeLatexAction_->setChecked(
        buildEngine_ == TeXEngine::XeLaTeX);

    timeout30SecondsAction_->setChecked(
        buildTimeoutMilliseconds_ == 30 * 1000);
    timeout2MinutesAction_->setChecked(
        buildTimeoutMilliseconds_ == 2 * 60 * 1000);
    timeout5MinutesAction_->setChecked(
        buildTimeoutMilliseconds_ == 5 * 60 * 1000);
    timeout10MinutesAction_->setChecked(
        buildTimeoutMilliseconds_ == 10 * 60 * 1000);
    timeoutDisabledAction_->setChecked(
        buildTimeoutMilliseconds_ <= 0);

    if (!buildTimeoutActionGroup_->checkedAction()) {
        buildTimeoutMilliseconds_ =
            10 * 60 * 1000;
        timeout10MinutesAction_->setChecked(true);
        settingsService_.setValue(
            "build/timeoutMilliseconds",
            buildTimeoutMilliseconds_);
    }

    updateBuildActions();

    auto* viewMenu = menuBar()->addMenu("&View");

    projectPanelAction_ = viewMenu->addAction("&Project Panel");
    projectPanelAction_->setCheckable(true);
    projectPanelAction_->setChecked(true);
    connect(projectPanelAction_, &QAction::toggled, projectPanel_, &QWidget::setVisible);

    previewPanelAction_ = viewMenu->addAction("&PDF Preview");
    previewPanelAction_->setCheckable(true);
    previewPanelAction_->setChecked(true);
    connect(previewPanelAction_, &QAction::toggled, previewPanel_, &QWidget::setVisible);

    statusBarAction_ = viewMenu->addAction("&Status Bar");
    statusBarAction_->setCheckable(true);
    statusBarAction_->setChecked(true);
    connect(statusBarAction_, &QAction::toggled, statusBar(), &QWidget::setVisible);

    viewMenu->addAction(
        buildOutputDock_->toggleViewAction());

    viewMenu->addSeparator();

    auto* resetLayoutAction = viewMenu->addAction("&Reset Layout");
    connect(resetLayoutAction, &QAction::triggered, this, &MainWindow::resetWorkspaceLayout);

    auto* helpMenu = menuBar()->addMenu("&Help");

    auto* aboutAction = helpMenu->addAction("&About Native TeX Editor");
    connect(aboutAction, &QAction::triggered, this, [this] {
        QMessageBox::about(
            this,
            "About Native TeX Editor",
            "Native TeX Editor\n\n"
            "Phase 4 — PDF + SyncTeX Backend\n"
            "Direct TeX orchestration + supervised manual builds\n"
            "Generation-safe Qt PDF preview + SyncTeX navigation\n"
            "Scintilla 5.6.6 + Lexilla 5.5.3");
    });

    helpMenu->addSeparator();

    auto* aboutQtAction = helpMenu->addAction("About &Qt");
    connect(aboutQtAction, &QAction::triggered, qApp, &QApplication::aboutQt);

    updateProjectActions();
}

void MainWindow::createStatusBar()
{
    statusBar()->showMessage("Ready");
}


void MainWindow::configureRecovery()
{
    recoveryTimer_ = new QTimer(this);
    recoveryTimer_->setInterval(RecoveryIntervalMilliseconds);

    connect(
        recoveryTimer_,
        &QTimer::timeout,
        this,
        &MainWindow::persistRecoverySnapshotIfNeeded);

    recoveryTimer_->start();
}

void MainWindow::processStartupRecovery()
{
    QString errorMessage;
    const std::optional<RecoverySnapshot> snapshot =
        recoveryService_.loadSnapshot(errorMessage);

    if (!errorMessage.isEmpty()) {
        QMessageBox::warning(
            this,
            "Recovery Data Could Not Be Read",
            errorMessage
                + "\n\nThe recovery file has been left untouched. "
                  "Automatic recovery snapshots are disabled for this "
                  "session to avoid overwriting it.");
        statusBar()->showMessage(
            "Recovery data unreadable — snapshot preserved",
            8000);
        return;
    }

    if (!snapshot.has_value()) {
        recoveryManagementActive_ = true;
        return;
    }

    while (true) {
        QMessageBox box(
            QMessageBox::Warning,
            "Recover Unsaved Work",
            "Unsaved work from a previous interrupted session was found.",
            QMessageBox::NoButton,
            this);

        const QString sourceLabel =
            snapshot->originalFilePath.isEmpty()
                ? "Untitled document"
                : QDir::toNativeSeparators(
                      snapshot->originalFilePath);

        const QString timestamp =
            QLocale().toString(
                snapshot->timestampUtc.toLocalTime(),
                QLocale::ShortFormat);

        box.setInformativeText(
            QString(
                "Source: %1\nRecovery snapshot: %2\n\n"
                "Restore loads the recovered text without overwriting "
                "the saved file. Compare is read-only. Discard removes "
                "the recovery snapshot.")
                .arg(sourceLabel, timestamp));

        auto* restoreButton =
            box.addButton("Restore", QMessageBox::AcceptRole);
        auto* compareButton =
            box.addButton("Compare", QMessageBox::ActionRole);
        auto* discardButton =
            box.addButton("Discard", QMessageBox::DestructiveRole);

        box.setDefaultButton(restoreButton);
        box.exec();

        if (box.clickedButton() == compareButton) {
            const RecoveryComparisonDialog::Decision decision =
                showRecoveryComparison(*snapshot);

            if (decision
                == RecoveryComparisonDialog::Decision::Restore) {
                recoveryManagementActive_ = true;
                restoreRecoverySnapshot(*snapshot);
                return;
            }

            if (decision
                == RecoveryComparisonDialog::Decision::Discard) {
                recoveryManagementActive_ = true;
                clearRecoverySnapshot();
                statusBar()->showMessage(
                    "Recovery snapshot discarded",
                    5000);
                return;
            }

            // Close/Back returns to the original recovery decision.
            continue;
        }

        if (box.clickedButton() == restoreButton) {
            recoveryManagementActive_ = true;
            restoreRecoverySnapshot(*snapshot);
            return;
        }

        if (box.clickedButton() == discardButton) {
            recoveryManagementActive_ = true;
            clearRecoverySnapshot();
            statusBar()->showMessage(
                "Recovery snapshot discarded",
                5000);
            return;
        }

        // Closing the decision dialog is deliberately not interpreted
        // as Discard. Re-open it until an explicit safe choice is made.
    }
}

void MainWindow::persistRecoverySnapshotIfNeeded()
{
    if (!recoveryManagementActive_
        || !documentSession_.isDirty()) {
        return;
    }

    const QString text = editorBackend_->text();
    const QString originalFilePath =
        documentSession_.hasFilePath()
            ? documentSession_.filePath()
            : QString();

    const QByteArray digest =
        recoveryDigest(originalFilePath, text);

    if (digest == lastRecoveryDigest_)
        return;

    QString errorMessage;

    if (!recoveryService_.writeSnapshot(
            originalFilePath,
            text,
            errorMessage)) {
        qWarning().noquote() << errorMessage;
        statusBar()->showMessage(
            "Warning: recovery snapshot could not be written",
            7000);
        return;
    }

    lastRecoveryDigest_ = digest;
    qInfo() << "Recovery snapshot updated for"
            << (originalFilePath.isEmpty()
                    ? QString("Untitled")
                    : originalFilePath);
}

void MainWindow::clearRecoverySnapshot()
{
    if (!recoveryManagementActive_)
        return;

    QString errorMessage;

    if (!recoveryService_.discardSnapshot(errorMessage)) {
        qWarning().noquote() << errorMessage;
        return;
    }

    lastRecoveryDigest_.clear();
}

void MainWindow::restoreRecoverySnapshot(
    const RecoverySnapshot& snapshot)
{
    recoveredBaselineDirty_ = true;

    if (snapshot.originalFilePath.isEmpty())
        documentSession_.resetUntitled();
    else
        documentSession_.setFilePath(snapshot.originalFilePath);

    editorBackend_->setText(snapshot.text);
    documentSession_.setDirty(true);

    lastRecoveryDigest_ =
        recoveryDigest(
            snapshot.originalFilePath,
            snapshot.text);

    updateWindowTitle();
    updateProjectActions();

    statusBar()->showMessage(
        "Recovered unsaved work — save explicitly to keep it",
        10000);

    editorBackend_->focusEditor();
}

RecoveryComparisonDialog::Decision
MainWindow::showRecoveryComparison(
    const RecoverySnapshot& snapshot)
{
    QString savedLabel;
    QString savedText;

    if (snapshot.originalFilePath.isEmpty()) {
        savedLabel = "Saved version — none (Untitled)";
        savedText =
            "This recovered document had not been saved to a file.";
    } else if (!QFileInfo::exists(snapshot.originalFilePath)) {
        savedLabel = "Saved version — file no longer exists";
        savedText =
            QString("The original file is no longer present:\n%1")
                .arg(QDir::toNativeSeparators(
                    snapshot.originalFilePath));
    } else {
        QString errorMessage;
        savedLabel =
            QString("Saved on disk — %1")
                .arg(QFileInfo(
                    snapshot.originalFilePath).fileName());

        if (!documentFileService_.readUtf8(
                snapshot.originalFilePath,
                savedText,
                errorMessage)) {
            savedText = errorMessage;
        }
    }

    const QString recoveredLabel =
        QString("Recovered snapshot — %1")
            .arg(
                QLocale().toString(
                    snapshot.timestampUtc.toLocalTime(),
                    QLocale::ShortFormat));

    RecoveryComparisonDialog dialog(
        savedLabel,
        savedText,
        recoveredLabel,
        snapshot.text,
        this);
    dialog.exec();
    return dialog.decision();
}

void MainWindow::newDocument()
{
    if (!maybeSaveCurrentDocument())
        return;

    recoveredBaselineDirty_ = false;
    clearRecoverySnapshot();

    documentSession_.resetUntitled();
    editorBackend_->setText({});
    documentSession_.setDirty(false);

    updateWindowTitle();
    updateProjectActions();
    refreshPreviewTarget();
    statusBar()->showMessage("New document");
    editorBackend_->focusEditor();
}

void MainWindow::openDocument()
{
    if (!maybeSaveCurrentDocument())
        return;

    const QString initialDirectory =
        documentSession_.hasFilePath()
            ? QFileInfo(documentSession_.filePath()).absolutePath()
            : projectService_.isOpen()
                ? projectService_.rootPath()
                : settingsService_.value(
                      "files/lastDirectory",
                      QDir::homePath()).toString();

    const QString filePath = QFileDialog::getOpenFileName(
        this,
        "Open Document",
        initialDirectory,
        documentFilter());

    if (filePath.isEmpty())
        return;

    openDocumentPath(filePath);
}

void MainWindow::openDocumentPath(const QString& filePath)
{
    if (!QFileInfo::exists(filePath)) {
        removeRecentFile(filePath);
        QMessageBox::warning(
            this,
            "File Not Found",
            QString("The file no longer exists:\n%1").arg(filePath));
        return;
    }

    QString text;
    QString errorMessage;

    if (!documentFileService_.readUtf8(filePath, text, errorMessage)) {
        QMessageBox::critical(this, "Open Failed", errorMessage);
        return;
    }

    recoveredBaselineDirty_ = false;
    clearRecoverySnapshot();

    documentSession_.setFilePath(filePath);
    editorBackend_->setText(text);
    documentSession_.setDirty(false);

    settingsService_.setValue(
        "files/lastDirectory",
        QFileInfo(filePath).absolutePath());
    addRecentFile(filePath);

    updateWindowTitle();
    updateProjectActions();
    refreshPreviewTarget();
    statusBar()->showMessage(
        QString("Opened %1").arg(documentSession_.displayName()));
    editorBackend_->focusEditor();
}

bool MainWindow::saveDocument()
{
    if (!documentSession_.hasFilePath())
        return saveDocumentAs();

    return saveDocumentToPath(documentSession_.filePath());
}

bool MainWindow::saveDocumentAs()
{
    const QString initialPath =
        documentSession_.hasFilePath()
            ? documentSession_.filePath()
            : QDir(
                  projectService_.isOpen()
                      ? projectService_.rootPath()
                      : settingsService_.value(
                            "files/lastDirectory",
                            QDir::homePath()).toString())
                  .filePath("Untitled.tex");

    const QString filePath = QFileDialog::getSaveFileName(
        this,
        "Save Document As",
        initialPath,
        documentFilter());

    if (filePath.isEmpty())
        return false;

    return saveDocumentToPath(filePath);
}

bool MainWindow::saveDocumentToPath(const QString& filePath)
{
    QString errorMessage;

    if (!documentFileService_.writeUtf8Atomic(
            filePath,
            editorBackend_->text(),
            errorMessage)) {
        QMessageBox::critical(this, "Save Failed", errorMessage);
        return false;
    }

    documentSession_.setFilePath(filePath);
    recoveredBaselineDirty_ = false;
    editorBackend_->markSaved();
    documentSession_.setDirty(false);
    clearRecoverySnapshot();

    settingsService_.setValue(
        "files/lastDirectory",
        QFileInfo(filePath).absolutePath());
    addRecentFile(filePath);

    updateWindowTitle();
    updateProjectActions();
    refreshPreviewTarget();
    statusBar()->showMessage(
        QString("Saved %1").arg(documentSession_.displayName()));
    return true;
}

void MainWindow::closeDocument()
{
    if (!maybeSaveCurrentDocument())
        return;

    recoveredBaselineDirty_ = false;
    clearRecoverySnapshot();

    documentSession_.resetUntitled();
    editorBackend_->setText({});
    documentSession_.setDirty(false);

    updateWindowTitle();
    updateProjectActions();
    refreshPreviewTarget();
    statusBar()->showMessage("Document closed");
    editorBackend_->focusEditor();
}

bool MainWindow::maybeSaveCurrentDocument()
{
    if (!documentSession_.isDirty())
        return true;

    const QMessageBox::StandardButton result = QMessageBox::warning(
        this,
        "Unsaved Changes",
        QString("Save changes to %1?")
            .arg(documentSession_.displayName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);

    if (result == QMessageBox::Save)
        return saveDocument();

    if (result == QMessageBox::Cancel)
        return false;

    recoveredBaselineDirty_ = false;
    clearRecoverySnapshot();
    return true;
}

void MainWindow::updateWindowTitle()
{
    const QString dirtyMarker = documentSession_.isDirty() ? " *" : "";

    setWindowTitle(
        QString("%1%2 — Native TeX Editor")
            .arg(documentSession_.displayName(), dirtyMarker));
}

void MainWindow::loadRecentFiles()
{
    recentFiles_ =
        settingsService_.value("files/recentFiles").toStringList();

    QStringList normalized;

    for (const QString& path : recentFiles_) {
        const QString absolutePath = QFileInfo(path).absoluteFilePath();

        if (!normalized.contains(absolutePath, Qt::CaseInsensitive))
            normalized.append(absolutePath);

        if (normalized.size() >= MaximumRecentFiles)
            break;
    }

    recentFiles_ = normalized;
    rebuildRecentFilesMenu();
}

void MainWindow::saveRecentFiles()
{
    settingsService_.setValue("files/recentFiles", recentFiles_);
    settingsService_.sync();
}

void MainWindow::addRecentFile(const QString& filePath)
{
    const QString absolutePath = QFileInfo(filePath).absoluteFilePath();

    for (auto it = recentFiles_.begin(); it != recentFiles_.end();) {
        if (QString::compare(*it, absolutePath, Qt::CaseInsensitive) == 0)
            it = recentFiles_.erase(it);
        else
            ++it;
    }

    recentFiles_.prepend(absolutePath);

    while (recentFiles_.size() > MaximumRecentFiles)
        recentFiles_.removeLast();

    saveRecentFiles();
    rebuildRecentFilesMenu();
}

void MainWindow::removeRecentFile(const QString& filePath)
{
    for (auto it = recentFiles_.begin(); it != recentFiles_.end();) {
        if (QString::compare(*it, filePath, Qt::CaseInsensitive) == 0)
            it = recentFiles_.erase(it);
        else
            ++it;
    }

    saveRecentFiles();
    rebuildRecentFilesMenu();
}

void MainWindow::rebuildRecentFilesMenu()
{
    if (!recentFilesMenu_)
        return;

    recentFilesMenu_->clear();

    if (recentFiles_.isEmpty()) {
        auto* emptyAction = recentFilesMenu_->addAction("(No Recent Files)");
        emptyAction->setEnabled(false);
        return;
    }

    for (int index = 0; index < recentFiles_.size(); ++index) {
        const QString path = recentFiles_.at(index);
        const QFileInfo info(path);

        QString label =
            QString("&%1 %2 — %3")
                .arg(index + 1)
                .arg(
                    info.fileName(),
                    QDir::toNativeSeparators(info.absolutePath()));

        auto* action = recentFilesMenu_->addAction(label);
        action->setToolTip(QDir::toNativeSeparators(path));

        connect(action, &QAction::triggered, this, [this, path] {
            if (!QFileInfo::exists(path)) {
                removeRecentFile(path);
                QMessageBox::warning(
                    this,
                    "File Not Found",
                    QString("The file no longer exists:\n%1").arg(path));
                return;
            }

            if (!maybeSaveCurrentDocument())
                return;

            openDocumentPath(path);
        });
    }

    recentFilesMenu_->addSeparator();

    auto* clearAction = recentFilesMenu_->addAction("Clear Recent Files");
    connect(clearAction, &QAction::triggered, this, [this] {
        recentFiles_.clear();
        saveRecentFiles();
        rebuildRecentFilesMenu();
    });
}

void MainWindow::openProject()
{
    const QString initialDirectory =
        projectService_.isOpen()
            ? projectService_.rootPath()
            : settingsService_.value(
                  "projects/lastDirectory",
                  QDir::homePath()).toString();

    const QString rootPath = QFileDialog::getExistingDirectory(
        this,
        "Open Project Folder",
        initialDirectory,
        QFileDialog::ShowDirsOnly);

    if (rootPath.isEmpty())
        return;

    openProjectPath(rootPath);
}

void MainWindow::openProjectPath(
    const QString& rootPath,
    bool addToRecent)
{
    QString errorMessage;

    if (!projectService_.openProject(rootPath, errorMessage)) {
        if (addToRecent)
            removeRecentProject(rootPath);

        QMessageBox::critical(this, "Open Project Failed", errorMessage);
        return;
    }

    settingsService_.setValue(
        "projects/lastRoot",
        projectService_.rootPath());
    settingsService_.setValue(
        "projects/lastDirectory",
        QFileInfo(projectService_.rootPath()).absolutePath());

    if (addToRecent)
        addRecentProject(projectService_.rootPath());

    restoreProjectMainDocument();
    syncTeXService_.cancel();
    previewService_.clear();
    updateProjectPanel();
    updateProjectActions();
    updateBuildActions();
    settingsService_.sync();

    statusBar()->showMessage(
        QString("Project opened: %1")
            .arg(projectService_.displayName()));
}

void MainWindow::closeProject()
{
    if (!projectService_.isOpen())
        return;

    const QString displayName = projectService_.displayName();

    projectService_.closeProject();
    syncTeXService_.cancel();
    previewService_.clear();
    settingsService_.setValue("projects/lastRoot", QString());
    settingsService_.sync();

    updateProjectPanel();
    updateProjectActions();
    updateBuildActions();

    statusBar()->showMessage(
        QString("Project closed: %1").arg(displayName));
}

void MainWindow::restoreLastProject()
{
    const QString rootPath =
        settingsService_.value("projects/lastRoot").toString();

    if (rootPath.isEmpty())
        return;

    if (!QFileInfo(rootPath).isDir()) {
        settingsService_.setValue("projects/lastRoot", QString());
        settingsService_.sync();
        return;
    }

    openProjectPath(rootPath, false);
}

void MainWindow::restoreProjectMainDocument()
{
    if (!projectService_.isOpen())
        return;

    const QString storedPath =
        settingsService_.value(
            mainDocumentSettingsKey(projectService_.rootPath()))
            .toString();

    if (storedPath.isEmpty())
        return;

    if (!projectService_.setMainDocumentPath(storedPath)) {
        settingsService_.setValue(
            mainDocumentSettingsKey(projectService_.rootPath()),
            QString());
    }
}

void MainWindow::setCurrentDocumentAsMain()
{
    if (!projectService_.isOpen()
        || !documentSession_.hasFilePath()
        || !isMainDocumentCandidate(documentSession_.filePath())
        || !projectService_.containsPath(documentSession_.filePath())) {
        return;
    }

    if (!projectService_.setMainDocumentPath(
            documentSession_.filePath())) {
        return;
    }

    settingsService_.setValue(
        mainDocumentSettingsKey(projectService_.rootPath()),
        projectService_.mainDocumentPath());
    settingsService_.sync();

    syncTeXService_.cancel();
    previewService_.clear();
    updateProjectPanel();
    updateProjectActions();
    updateBuildActions();

    statusBar()->showMessage(
        QString("Main document: %1")
            .arg(projectService_.relativePath(
                projectService_.mainDocumentPath())));
}

void MainWindow::clearMainDocument()
{
    if (!projectService_.isOpen())
        return;

    settingsService_.setValue(
        mainDocumentSettingsKey(projectService_.rootPath()),
        QString());
    settingsService_.sync();

    projectService_.clearMainDocumentPath();
    syncTeXService_.cancel();
    previewService_.clear();
    updateProjectPanel();
    updateProjectActions();
    updateBuildActions();

    statusBar()->showMessage("Main document cleared");
}

void MainWindow::updateProjectPanel()
{
    if (!projectService_.isOpen()) {
        projectNameLabel_->clear();
        projectPathLabel_->clear();
        projectMainDocumentLabel_->clear();

        projectNameLabel_->hide();
        projectPathLabel_->hide();
        projectMainDocumentLabel_->hide();

        projectTreeView_->setRootIndex(QModelIndex());
        projectStack_->setCurrentWidget(projectEmptyPage_);
        return;
    }

    const QString rootPath = projectService_.rootPath();

    projectNameLabel_->setText(projectService_.displayName());
    projectPathLabel_->setText(QDir::toNativeSeparators(rootPath));
    projectPathLabel_->setToolTip(QDir::toNativeSeparators(rootPath));

    const QString mainPath = projectService_.mainDocumentPath();

    if (mainPath.isEmpty()) {
        projectMainDocumentLabel_->setText("Main: not set");
        projectMainDocumentLabel_->setToolTip({});
    } else {
        const QString relative = projectService_.relativePath(mainPath);
        projectMainDocumentLabel_->setText(
            QString("Main: %1").arg(QDir::toNativeSeparators(relative)));
        projectMainDocumentLabel_->setToolTip(
            QDir::toNativeSeparators(mainPath));
    }

    projectNameLabel_->show();
    projectPathLabel_->show();
    projectMainDocumentLabel_->show();

    const QModelIndex rootIndex =
        projectFileModel_->setRootPath(rootPath);
    projectTreeView_->setRootIndex(rootIndex);
    projectStack_->setCurrentWidget(projectTreePage_);
}

void MainWindow::updateProjectActions()
{
    const bool projectOpen = projectService_.isOpen();

    if (closeProjectAction_)
        closeProjectAction_->setEnabled(projectOpen);

    const bool currentCanBeMain =
        projectOpen
        && documentSession_.hasFilePath()
        && projectService_.containsPath(documentSession_.filePath())
        && isMainDocumentCandidate(documentSession_.filePath());

    if (setMainDocumentAction_)
        setMainDocumentAction_->setEnabled(currentCanBeMain);

    if (clearMainDocumentAction_) {
        clearMainDocumentAction_->setEnabled(
            projectOpen
            && !projectService_.mainDocumentPath().isEmpty());
    }
}

void MainWindow::handleProjectTreeDoubleClick(
    const QModelIndex& index)
{
    if (!index.isValid())
        return;

    const QFileInfo info = projectFileModel_->fileInfo(index);

    if (info.isDir())
        return;

    const QString filePath = info.absoluteFilePath();

    if (!isEditableProjectFile(filePath)) {
        statusBar()->showMessage(
            QString("This file type is not editable yet: %1")
                .arg(info.fileName()),
            4000);
        return;
    }

    if (!maybeSaveCurrentDocument())
        return;

    openDocumentPath(filePath);
}

void MainWindow::loadRecentProjects()
{
    recentProjects_ =
        settingsService_.value("projects/recentProjects").toStringList();

    QStringList normalized;

    for (const QString& path : recentProjects_) {
        const QString absolutePath = QFileInfo(path).absoluteFilePath();

        if (!normalized.contains(absolutePath, Qt::CaseInsensitive))
            normalized.append(absolutePath);

        if (normalized.size() >= MaximumRecentProjects)
            break;
    }

    recentProjects_ = normalized;
    rebuildRecentProjectsMenu();
}

void MainWindow::saveRecentProjects()
{
    settingsService_.setValue(
        "projects/recentProjects",
        recentProjects_);
    settingsService_.sync();
}

void MainWindow::addRecentProject(const QString& rootPath)
{
    const QString absolutePath =
        QFileInfo(rootPath).absoluteFilePath();

    for (auto it = recentProjects_.begin();
         it != recentProjects_.end();) {
        if (QString::compare(
                *it,
                absolutePath,
                Qt::CaseInsensitive) == 0) {
            it = recentProjects_.erase(it);
        } else {
            ++it;
        }
    }

    recentProjects_.prepend(absolutePath);

    while (recentProjects_.size() > MaximumRecentProjects)
        recentProjects_.removeLast();

    saveRecentProjects();
    rebuildRecentProjectsMenu();
}

void MainWindow::removeRecentProject(const QString& rootPath)
{
    for (auto it = recentProjects_.begin();
         it != recentProjects_.end();) {
        if (QString::compare(
                *it,
                rootPath,
                Qt::CaseInsensitive) == 0) {
            it = recentProjects_.erase(it);
        } else {
            ++it;
        }
    }

    saveRecentProjects();
    rebuildRecentProjectsMenu();
}

void MainWindow::rebuildRecentProjectsMenu()
{
    if (!recentProjectsMenu_)
        return;

    recentProjectsMenu_->clear();

    if (recentProjects_.isEmpty()) {
        auto* emptyAction =
            recentProjectsMenu_->addAction("(No Recent Projects)");
        emptyAction->setEnabled(false);
        return;
    }

    for (int index = 0;
         index < recentProjects_.size();
         ++index) {
        const QString path = recentProjects_.at(index);
        const QFileInfo info(path);

        QString displayName = info.fileName();
        if (displayName.isEmpty())
            displayName = QDir::toNativeSeparators(path);

        const QString parentPath =
            QDir::toNativeSeparators(info.absolutePath());

        auto* action = recentProjectsMenu_->addAction(
            QString("&%1 %2 — %3")
                .arg(index + 1)
                .arg(displayName, parentPath));

        action->setToolTip(QDir::toNativeSeparators(path));

        connect(action, &QAction::triggered, this, [this, path] {
            if (!QFileInfo(path).isDir()) {
                removeRecentProject(path);
                QMessageBox::warning(
                    this,
                    "Project Not Found",
                    QString("The project folder no longer exists:\n%1")
                        .arg(QDir::toNativeSeparators(path)));
                return;
            }

            openProjectPath(path);
        });
    }

    recentProjectsMenu_->addSeparator();

    auto* clearAction =
        recentProjectsMenu_->addAction("Clear Recent Projects");
    connect(clearAction, &QAction::triggered, this, [this] {
        recentProjects_.clear();
        saveRecentProjects();
        rebuildRecentProjectsMenu();
    });
}

void MainWindow::configureBuildOutput()
{
    buildOutputDock_ =
        new QDockWidget("Build Output", this);
    buildOutputDock_->setObjectName(
        "BuildOutputDock");
    buildOutputDock_->setAllowedAreas(
        Qt::BottomDockWidgetArea
        | Qt::TopDockWidgetArea);

    buildOutputEdit_ =
        new QPlainTextEdit(buildOutputDock_);
    buildOutputEdit_->setReadOnly(true);
    buildOutputEdit_->setLineWrapMode(
        QPlainTextEdit::NoWrap);
    buildOutputEdit_->setMaximumBlockCount(5000);

    buildOutputDock_->setWidget(
        buildOutputEdit_);

    addDockWidget(
        Qt::BottomDockWidgetArea,
        buildOutputDock_);

    buildOutputDock_->hide();
}

QString MainWindow::resolvedBuildTarget() const
{
    if (projectService_.isOpen()
        && !projectService_.mainDocumentPath().isEmpty()) {
        return projectService_.mainDocumentPath();
    }

    if (documentSession_.hasFilePath()
        && isMainDocumentCandidate(
            documentSession_.filePath())) {
        return documentSession_.filePath();
    }

    return {};
}

bool MainWindow::saveRelevantDirtyDocumentBeforeBuild()
{
    if (!documentSession_.isDirty())
        return true;

    if (!documentSession_.hasFilePath())
        return true;

    const QString targetPath =
        resolvedBuildTarget();

    if (targetPath.isEmpty())
        return true;

    const QString currentPath =
        QFileInfo(documentSession_.filePath())
            .absoluteFilePath();

    const bool currentIsTarget =
        QString::compare(
            currentPath,
            QFileInfo(targetPath)
                .absoluteFilePath(),
            Qt::CaseInsensitive) == 0;

    const bool currentIsProjectSource =
        projectService_.isOpen()
        && projectService_.containsPath(
            currentPath);

    if (!currentIsTarget
        && !currentIsProjectSource) {
        return true;
    }

    return saveDocument();
}

void MainWindow::startBuild()
{
    if (buildService_.isBuilding())
        return;

    const QString targetPath =
        resolvedBuildTarget();

    if (targetPath.isEmpty()) {
        QMessageBox::information(
            this,
            "No Build Target",
            "No TeX build target is available.\n\n"
            "Set a project Main document, or open a "
            "saved .tex/.ltx document.");
        return;
    }

    if (!saveRelevantDirtyDocumentBeforeBuild())
        return;

    previewService_.clearIfTargetChanged(targetPath);

    environmentService_.refresh();

    if (!environmentService_.canBuildWith(
            buildEngine_)) {
        showSystemCheck();
        return;
    }

    buildOutputEdit_->clear();
    showBuildOutput();

    const QFileInfo targetInfo(targetPath);
    const TeXEnvironmentSnapshot environment =
        environmentService_.snapshot();

    appendBuildOutput(
        QString(
            "=== Build started ===\n"
            "Target: %1\n"
            "Engine: %2\n"
            "Backend: Direct TeX orchestration\n"
            "Distribution: %3\n"
            "Toolchain: %4\n"
            "Timeout: %5\n")
            .arg(
                QDir::toNativeSeparators(
                    targetInfo.absoluteFilePath()),
                texEngineDisplayName(buildEngine_),
                texDistributionDisplayName(
                    environment.distribution),
                environment.customToolchainPath.isEmpty()
                    ? QString("System PATH")
                    : QDir::toNativeSeparators(
                          environment.customToolchainPath),
                buildTimeoutLabel(
                    buildTimeoutMilliseconds_)));

#if !defined(NDEBUG)
    qInfo().noquote()
        << QString(
               "BUILD_METRIC_BEGIN "
               "backend=direct "
               "engine=%1 "
               "distribution=%2 "
               "target=\"%3\"")
               .arg(
                   texEngineDisplayName(buildEngine_),
                   texDistributionDisplayName(
                       environment.distribution),
                   QDir::toNativeSeparators(
                       targetInfo.absoluteFilePath()));
#endif

    BuildRequest request;
    request.targetPath = targetPath;
    request.engine = buildEngine_;
    request.timeoutMilliseconds =
        buildTimeoutMilliseconds_;

    QPointer<MainWindow> self(this);

    const bool accepted =
        buildService_.startBuild(
            request,
            [self](const QString& text) {
                if (self)
                    self->appendBuildOutput(text);
            },
            [self](const BuildResult& result) {
                if (self)
                    self->handleBuildFinished(result);
            });

    if (!accepted) {
        appendBuildOutput(
            "\nBuild could not start because "
            "another build is active.\n");
        return;
    }

    updateBuildActions();

    if (buildService_.isBuilding()) {
        statusBar()->showMessage(
            QString("Building %1 with %2...")
                .arg(
                    targetInfo.fileName(),
                    texEngineDisplayName(
                        buildEngine_)));
    }
}

void MainWindow::stopBuild()
{
    if (!buildService_.isBuilding())
        return;

    appendBuildOutput(
        "\n=== Cancelling build ===\n");

    statusBar()->showMessage(
        "Cancelling build...");

    buildService_.cancelBuild();
}

void MainWindow::showSystemCheck()
{
    environmentService_.refresh();

    const TeXEnvironmentSnapshot environment =
        environmentService_.snapshot();

    const auto lineFor =
        [](const ToolInfo& tool) {
            return QString("%1: %2")
                .arg(
                    tool.displayName,
                    tool.available()
                        ? QDir::toNativeSeparators(
                              tool.path)
                        : QString("NOT FOUND"));
        };

    const bool directReady =
        environmentService_.canBuildWith(
            buildEngine_);

    const bool latexmkUsable =
        environment.latexmk.available()
        && environment.perl.available();

    const QString toolchainSource =
        environment.customToolchainPath.isEmpty()
            ? QString("System PATH")
            : QDir::toNativeSeparators(
                  environment.customToolchainPath);

    const QString report =
        QString(
            "TeX Environment\n\n"
            "Distribution: %1\n"
            "Toolchain source: %2\n"
            "Primary backend: Direct TeX orchestration\n"
            "Direct build status: %3\n\n"
            "Direct-build tools\n"
            "%4\n"
            "%5\n"
            "%6\n"
            "%7\n"
            "%8\n\n"
            "Optional automation tools\n"
            "%9\n"
            "%10\n"
            "latexmk optional status: %11\n\n"
            "Distribution evidence\n"
            "%12\n"
            "%13\n"
            "%14\n"
            "%15\n\n"
            "Selected engine: %16\n"
            "Build timeout: %17\n\n"
            "Synchronization tool\n"
            "%18")
            .arg(
                texDistributionDisplayName(
                    environment.distribution),
                toolchainSource,
                directReady
                    ? QString("READY")
                    : QString("NOT READY"),
                lineFor(environment.pdflatex),
                lineFor(environment.lualatex),
                lineFor(environment.xelatex),
                lineFor(environment.bibtex),
                lineFor(environment.biber),
                lineFor(environment.latexmk),
                lineFor(environment.perl),
                latexmkUsable
                    ? QString("USABLE")
                    : QString(
                          "UNAVAILABLE (optional)"),
                lineFor(environment.initexmf),
                lineFor(environment.miktex),
                lineFor(environment.kpsewhich),
                lineFor(environment.tlmgr),
                texEngineDisplayName(buildEngine_),
                buildTimeoutLabel(
                    buildTimeoutMilliseconds_),
                lineFor(environment.synctex));

    QMessageBox::information(
        this,
        "TeX System Check",
        report);
}

void MainWindow::showBuildOutput()
{
    buildOutputDock_->show();
    buildOutputDock_->raise();
}

void MainWindow::appendBuildOutput(
    const QString& text)
{
    if (text.isEmpty())
        return;

    QTextCursor cursor =
        buildOutputEdit_->textCursor();

    cursor.movePosition(
        QTextCursor::End);
    cursor.insertText(text);

    buildOutputEdit_->setTextCursor(cursor);
    buildOutputEdit_->ensureCursorVisible();
}

void MainWindow::handleBuildFinished(
    const BuildResult& result)
{
    const QString elapsed =
        formatBuildElapsed(
            result.elapsedMilliseconds);

#if !defined(NDEBUG)
    QString debugOutcome = "failed";

    if (result.success)
        debugOutcome = "success";
    else if (result.cancelled)
        debugOutcome = "cancelled";
    else if (result.timedOut)
        debugOutcome = "timed_out";
    else if (result.crashed)
        debugOutcome = "crashed";

    qInfo().noquote()
        << QString(
               "BUILD_METRIC_END "
               "outcome=%1 "
               "engine=%2 "
               "elapsed_ms=%3 "
               "exit_code=%4 "
               "crashed=%5 "
               "target=\"%6\"")
               .arg(
                   debugOutcome,
                   texEngineDisplayName(buildEngine_))
               .arg(result.elapsedMilliseconds)
               .arg(result.exitCode)
               .arg(result.crashed ? "yes" : "no")
               .arg(
                   QDir::toNativeSeparators(
                       result.targetPath));
#endif

    if (result.cancelled) {
        appendBuildOutput(
            QString(
                "\n=== Build cancelled ===\n"
                "Elapsed: %1\n")
                .arg(elapsed));

        statusBar()->showMessage(
            QString(
                "Build cancelled — %1")
                .arg(elapsed),
            5000);
    } else if (result.timedOut) {
        appendBuildOutput(
            QString(
                "\n=== Build timed out ===\n"
                "Elapsed: %1\n")
                .arg(elapsed));

        statusBar()->showMessage(
            QString(
                "Build timed out — %1")
                .arg(elapsed),
            7000);
    } else if (result.success) {
        QString previewError;
        const bool previewUpdated =
            previewService_.activateSuccessfulBuild(
                result,
                previewError);

        appendBuildOutput(
            QString(
                "\n=== Build succeeded ===\n"
                "Elapsed: %1\n"
                "PDF: %2\n"
                "Preview: %3\n")
                .arg(
                    elapsed,
                    QDir::toNativeSeparators(
                        result.pdfPath),
                    previewUpdated
                        ? QString("PDF preview updated from cached generation")
                        : QString("last-good generation retained")));

        if (!previewUpdated) {
            appendBuildOutput(
                QString("Preview warning: %1\n")
                    .arg(previewError));
        } else {
            const auto generation =
                previewService_.currentGeneration();
            appendBuildOutput(
                generation.has_value() && generation->hasSyncTeX()
                    ? QString("SyncTeX: cached with PDF generation\n")
                    : QString(
                          "SyncTeX: no matching mapping file; "
                          "preview remains available\n"));
        }

        statusBar()->showMessage(
            QString(
                "Build succeeded — %1")
                .arg(elapsed),
            7000);
    } else {
        QString summary =
            QString(
                "\n=== Build failed ===\n"
                "Elapsed: %1\n"
                "Exit code: %2\n"
                "Process crashed: %3\n")
                .arg(elapsed)
                .arg(result.exitCode)
                .arg(
                    result.crashed
                        ? "yes"
                        : "no");

        if (!result.errorMessage.isEmpty()) {
            summary +=
                QString("Error: %1\n")
                    .arg(
                        result.errorMessage);
        }

        appendBuildOutput(summary);

        statusBar()->showMessage(
            QString(
                "Build failed — %1 — see Build Output")
                .arg(elapsed),
            7000);
    }

    updateBuildActions();

    if (closeAfterBuildStops_) {
        closeAfterBuildStops_ = false;
        QTimer::singleShot(
            0,
            this,
            &QWidget::close);
    }
}

std::optional<SyncTeXRequestContext>
MainWindow::syncTeXContext(QString& errorMessage) const
{
    const std::optional<PreviewGeneration> generation =
        previewService_.currentGeneration();

    if (!generation.has_value()) {
        errorMessage = "Build the current target before synchronizing.";
        return std::nullopt;
    }

    if (!generation->hasSyncTeX()) {
        errorMessage =
            "The active preview generation has no matching SyncTeX mapping file.";
        return std::nullopt;
    }

    const ToolInfo synctex =
        environmentService_.snapshot().synctex;
    if (!synctex.available()) {
        errorMessage =
            "The SyncTeX executable is not available in the selected toolchain.";
        return std::nullopt;
    }

    SyncTeXRequestContext context;
    context.generation = *generation;
    context.executablePath = synctex.path;
    errorMessage.clear();
    return context;
}

void MainWindow::syncSourceToPdf()
{
    if (syncTeXService_.isRunning()) {
        statusBar()->showMessage(
            "A synchronization query is already running",
            3000);
        return;
    }

    if (!documentSession_.hasFilePath()) {
        statusBar()->showMessage(
            "Save the source document before synchronizing",
            5000);
        return;
    }

    QString errorMessage;
    const auto context = syncTeXContext(errorMessage);
    if (!context.has_value()) {
        statusBar()->showMessage(errorMessage, 6000);
        return;
    }

    const QFileInfo sourceInfo(documentSession_.filePath());
    const QString canonicalSource = sourceInfo.canonicalFilePath();

    SourcePosition source;
    source.filePath =
        canonicalSource.isEmpty()
            ? sourceInfo.absoluteFilePath()
            : canonicalSource;
    source.line = editorBackend_->currentLine();
    source.column = editorBackend_->currentColumn();

    const QString generationId = context->generation.id;
    QPointer<MainWindow> self(this);

    const bool accepted = syncTeXService_.forward(
        *context,
        source,
        [self, generationId](const SyncTeXResult& result) {
            if (self)
                self->handleForwardSyncResult(generationId, result);
        });

    if (!accepted) {
        statusBar()->showMessage(
            "Unable to start the SyncTeX forward query",
            6000);
        return;
    }

    updateBuildActions();
    statusBar()->showMessage("Synchronizing source to PDF...");
}

void MainWindow::handleInverseSyncRequest(
    const PdfPosition& position)
{
    if (syncTeXService_.isRunning()) {
        statusBar()->showMessage(
            "A synchronization query is already running",
            3000);
        return;
    }

    QString errorMessage;
    const auto context = syncTeXContext(errorMessage);
    if (!context.has_value()) {
        statusBar()->showMessage(errorMessage, 6000);
        return;
    }

    const QString generationId = context->generation.id;
    QPointer<MainWindow> self(this);

    const bool accepted = syncTeXService_.inverse(
        *context,
        position,
        [self, generationId](const SyncTeXResult& result) {
            if (self)
                self->handleInverseSyncResult(generationId, result);
        });

    if (!accepted) {
        statusBar()->showMessage(
            "Unable to start the SyncTeX inverse query",
            6000);
        return;
    }

    updateBuildActions();
    statusBar()->showMessage("Synchronizing PDF to source...");
}

void MainWindow::handleForwardSyncResult(
    const QString& generationId,
    const SyncTeXResult& result)
{
    updateBuildActions();

    const auto active = previewService_.currentGeneration();
    if (!active.has_value() || active->id != generationId)
        return;

    if (!result.success) {
        appendBuildOutput(
            QString("\nSyncTeX forward query failed: %1\n%2")
                .arg(result.errorMessage, result.rawOutput));
        statusBar()->showMessage(
            "No PDF position was found for this source location",
            6000);
        return;
    }

    previewService_.navigateTo(result.pdfPosition);
    statusBar()->showMessage(
        QString("Source → PDF in %1 ms")
            .arg(result.elapsedMilliseconds),
        4000);
}

void MainWindow::handleInverseSyncResult(
    const QString& generationId,
    const SyncTeXResult& result)
{
    updateBuildActions();

    const auto active = previewService_.currentGeneration();
    if (!active.has_value() || active->id != generationId)
        return;

    if (!result.success) {
        appendBuildOutput(
            QString("\nSyncTeX inverse query failed: %1\n%2")
                .arg(result.errorMessage, result.rawOutput));
        statusBar()->showMessage(
            "No source location was found for this PDF position",
            6000);
        return;
    }

    QString sourcePath = QDir::fromNativeSeparators(
        result.sourcePosition.filePath);

    if (QFileInfo(sourcePath).isRelative()) {
        sourcePath = QDir(active->originalBuildDirectory)
            .absoluteFilePath(sourcePath);
    }

    const QFileInfo sourceInfo(sourcePath);
    const QString canonicalSource = sourceInfo.canonicalFilePath();
    sourcePath = QDir::cleanPath(
        canonicalSource.isEmpty()
            ? sourceInfo.absoluteFilePath()
            : canonicalSource);

    if (!QFileInfo(sourcePath).isFile()) {
        appendBuildOutput(
            QString("\nSyncTeX mapped to a missing source file: %1\n")
                .arg(QDir::toNativeSeparators(sourcePath)));
        statusBar()->showMessage(
            "SyncTeX mapped to a source file that no longer exists",
            6000);
        return;
    }

    QString currentPath;
    if (documentSession_.hasFilePath()) {
        const QFileInfo currentInfo(documentSession_.filePath());
        currentPath = currentInfo.canonicalFilePath();
        if (currentPath.isEmpty())
            currentPath = currentInfo.absoluteFilePath();
    }

    const QString destinationPath = sourcePath;
    const bool changesDocument =
        currentPath.isEmpty()
        || QString::compare(
               currentPath,
               destinationPath,
               Qt::CaseInsensitive) != 0;

    if (changesDocument) {
        if (!maybeSaveCurrentDocument()) {
            statusBar()->showMessage(
                "Inverse synchronization cancelled to protect unsaved work",
                5000);
            return;
        }

        openDocumentPath(sourcePath);
        if (!documentSession_.hasFilePath()
            || QString::compare(
                   QFileInfo(documentSession_.filePath()).absoluteFilePath(),
                   destinationPath,
                   Qt::CaseInsensitive) != 0) {
            return;
        }
    }

    editorBackend_->goToLineColumn(
        result.sourcePosition.line,
        result.sourcePosition.column);
    statusBar()->showMessage(
        QString("PDF → %1:%2 in %3 ms")
            .arg(
                QFileInfo(sourcePath).fileName())
            .arg(result.sourcePosition.line)
            .arg(result.elapsedMilliseconds),
        5000);
}

void MainWindow::refreshPreviewTarget()
{
    previewService_.clearIfTargetChanged(resolvedBuildTarget());
    updateBuildActions();
}

void MainWindow::updateBuildActions()
{
    if (!buildAction_
        || !stopBuildAction_) {
        return;
    }

    const bool building =
        buildService_.isBuilding();

    buildAction_->setEnabled(!building);
    stopBuildAction_->setEnabled(building);

    for (QAction* action :
         {pdfLatexAction_,
          luaLatexAction_,
          xeLatexAction_,
          timeout30SecondsAction_,
          timeout2MinutesAction_,
          timeout5MinutesAction_,
          timeout10MinutesAction_,
          timeoutDisabledAction_,
          chooseToolchainPathAction_}) {
        if (action)
            action->setEnabled(!building);
    }

    if (clearToolchainPathAction_) {
        clearToolchainPathAction_->setEnabled(
            !building
            && !environmentService_
                    .customToolchainPath()
                    .isEmpty());
    }

    if (syncSourceToPdfAction_) {
        const auto generation =
            previewService_.currentGeneration();
        syncSourceToPdfAction_->setEnabled(
            !building
            && !syncTeXService_.isRunning()
            && documentSession_.hasFilePath()
            && generation.has_value()
            && generation->hasSyncTeX());
    }
}

void MainWindow::setBuildEngine(
    TeXEngine engine)
{
    if (buildService_.isBuilding())
        return;

    buildEngine_ = engine;

    settingsService_.setValue(
        "build/engine",
        texEngineSettingValue(engine));
    settingsService_.sync();

    pdfLatexAction_->setChecked(
        engine == TeXEngine::PdfLaTeX);
    luaLatexAction_->setChecked(
        engine == TeXEngine::LuaLaTeX);
    xeLatexAction_->setChecked(
        engine == TeXEngine::XeLaTeX);

    statusBar()->showMessage(
        QString("Build engine: %1")
            .arg(
                texEngineDisplayName(engine)),
        4000);
}

void MainWindow::setBuildTimeoutMilliseconds(
    int timeoutMilliseconds)
{
    if (buildService_.isBuilding())
        return;

    buildTimeoutMilliseconds_ =
        timeoutMilliseconds > 0
            ? timeoutMilliseconds
            : 0;

    settingsService_.setValue(
        "build/timeoutMilliseconds",
        buildTimeoutMilliseconds_);
    settingsService_.sync();

    timeout30SecondsAction_->setChecked(
        buildTimeoutMilliseconds_ == 30 * 1000);
    timeout2MinutesAction_->setChecked(
        buildTimeoutMilliseconds_ == 2 * 60 * 1000);
    timeout5MinutesAction_->setChecked(
        buildTimeoutMilliseconds_ == 5 * 60 * 1000);
    timeout10MinutesAction_->setChecked(
        buildTimeoutMilliseconds_ == 10 * 60 * 1000);
    timeoutDisabledAction_->setChecked(
        buildTimeoutMilliseconds_ <= 0);

    statusBar()->showMessage(
        QString("Build timeout: %1")
            .arg(
                buildTimeoutLabel(
                    buildTimeoutMilliseconds_)),
        4000);
}

void MainWindow::chooseCustomToolchainPath()
{
    if (buildService_.isBuilding())
        return;

    const QString initialDirectory =
        environmentService_
            .customToolchainPath()
            .isEmpty()
        ? QDir::homePath()
        : environmentService_
              .customToolchainPath();

    const QString directory =
        QFileDialog::getExistingDirectory(
            this,
            "Choose TeX Bin Folder",
            initialDirectory,
            QFileDialog::ShowDirsOnly);

    if (directory.isEmpty())
        return;

    environmentService_
        .setCustomToolchainPath(directory);

    settingsService_.setValue(
        "build/customToolchainPath",
        environmentService_
            .customToolchainPath());
    settingsService_.sync();

    updateBuildActions();
    showSystemCheck();
}

void MainWindow::clearCustomToolchainPath()
{
    if (buildService_.isBuilding())
        return;

    environmentService_
        .setCustomToolchainPath({});

    settingsService_.setValue(
        "build/customToolchainPath",
        QString());
    settingsService_.sync();

    updateBuildActions();

    statusBar()->showMessage(
        "TeX toolchain: System PATH",
        4000);
}

void MainWindow::resetWorkspaceLayout()
{
    projectPanelAction_->setChecked(true);
    previewPanelAction_->setChecked(true);
    statusBarAction_->setChecked(true);

    const int width = workspaceSplitter_->width();
    const int projectWidth = width > 0 ? width * 20 / 100 : 240;
    const int editorWidth = width > 0 ? width * 50 / 100 : 600;
    const int previewWidth =
        width > 0
            ? width - projectWidth - editorWidth
            : 360;

    workspaceSplitter_->setSizes(
        {projectWidth, editorWidth, previewWidth});
    editorBackend_->focusEditor();
}

void MainWindow::restoreWindowState()
{
    const QByteArray geometry =
        settingsService_.value("window/geometry").toByteArray();

    if (!geometry.isEmpty())
        restoreGeometry(geometry);

    const QByteArray splitterState =
        settingsService_.value(
            "workspace/splitterState").toByteArray();

    if (!splitterState.isEmpty())
        workspaceSplitter_->restoreState(splitterState);
    else
        resetWorkspaceLayout();

    const bool projectPanelVisible =
        settingsService_.value(
            "view/projectPanelVisible",
            true).toBool();
    const bool previewPanelVisible =
        settingsService_.value(
            "view/previewPanelVisible",
            true).toBool();
    const bool statusBarVisible =
        settingsService_.value(
            "view/statusBarVisible",
            true).toBool();

    const bool buildOutputVisible =
        settingsService_.value(
            "view/buildOutputVisible",
            false).toBool();

    projectPanelAction_->setChecked(projectPanelVisible);
    previewPanelAction_->setChecked(previewPanelVisible);
    statusBarAction_->setChecked(statusBarVisible);

    projectPanel_->setVisible(projectPanelVisible);
    previewPanel_->setVisible(previewPanelVisible);
    statusBar()->setVisible(statusBarVisible);
    buildOutputDock_->setVisible(buildOutputVisible);
}

void MainWindow::saveWindowState()
{
    settingsService_.setValue(
        "window/geometry",
        saveGeometry());
    settingsService_.setValue(
        "workspace/splitterState",
        workspaceSplitter_->saveState());
    settingsService_.setValue(
        "view/projectPanelVisible",
        projectPanelAction_->isChecked());
    settingsService_.setValue(
        "view/previewPanelVisible",
        previewPanelAction_->isChecked());
    settingsService_.setValue(
        "view/statusBarVisible",
        statusBarAction_->isChecked());
    settingsService_.setValue(
        "view/buildOutputVisible",
        buildOutputDock_->isVisible());
    settingsService_.sync();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (buildService_.isBuilding()) {
        const QMessageBox::StandardButton answer =
            QMessageBox::question(
                this,
                "Build in Progress",
                "A TeX build is still running. "
                "Stop it and exit?",
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);

        if (answer != QMessageBox::Yes) {
            event->ignore();
            return;
        }

        closeAfterBuildStops_ = true;
        buildService_.cancelBuild();
        event->ignore();
        return;
    }

    if (!maybeSaveCurrentDocument()) {
        event->ignore();
        return;
    }

    syncTeXService_.cancel();
    recoveredBaselineDirty_ = false;
    clearRecoverySnapshot();
    saveWindowState();
    QMainWindow::closeEvent(event);
}
