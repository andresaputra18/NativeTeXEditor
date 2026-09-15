#include "pdf/QtPdfBackend.h"

#include "pdf/PdfPageLayoutMapper.h"

#include <QApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPdfDocument>
#include <QPdfPageNavigator>
#include <QPdfView>
#include <QScreen>
#include <QScrollBar>
#include <QSizePolicy>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <utility>

namespace
{
QString pdfErrorText(QPdfDocument::Error error)
{
    switch (error) {
    case QPdfDocument::Error::None:
        return {};
    case QPdfDocument::Error::Unknown:
        return "unknown PDF error";
    case QPdfDocument::Error::DataNotYetAvailable:
        return "PDF data is not yet available";
    case QPdfDocument::Error::FileNotFound:
        return "PDF file was not found";
    case QPdfDocument::Error::InvalidFileFormat:
        return "invalid PDF file format";
    case QPdfDocument::Error::IncorrectPassword:
        return "the PDF password is incorrect";
    case QPdfDocument::Error::UnsupportedSecurityScheme:
        return "the PDF security scheme is unsupported";
    }

    return "unrecognized PDF error";
}
}

QtPdfBackend::QtPdfBackend(QObject* parent)
    : QObject(parent)
{
    container_ = new QWidget;
    container_->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Expanding);

    auto* layout = new QVBoxLayout(container_);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* toolbar = new QWidget(container_);
    auto* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(8, 5, 8, 5);
    toolbarLayout->setSpacing(4);

    auto makeButton =
        [toolbar, toolbarLayout](const QString& text) {
            auto* button = new QToolButton(toolbar);
            button->setText(text);
            toolbarLayout->addWidget(button);
            return button;
        };

    QToolButton* previousButton = makeButton("Previous");
    QToolButton* nextButton = makeButton("Next");

    pageLabel_ = new QLabel("No PDF", toolbar);
    pageLabel_->setMinimumWidth(72);
    pageLabel_->setAlignment(Qt::AlignCenter);
    toolbarLayout->addWidget(pageLabel_);

    QToolButton* zoomOutButton = makeButton("-");
    QToolButton* zoomInButton = makeButton("+");
    QToolButton* fitWidthButton = makeButton("Fit W");
    QToolButton* fitPageButton = makeButton("Fit P");
    toolbarLayout->addStretch(1);

    view_ = new QPdfView(container_);
    view_->setPageMode(QPdfView::PageMode::MultiPage);
    view_->setZoomMode(QPdfView::ZoomMode::FitToWidth);
    view_->viewport()->installEventFilter(this);

    layout->addWidget(toolbar);
    layout->addWidget(view_, 1);

    connect(
        previousButton,
        &QToolButton::clicked,
        this,
        [this] { jumpRelative(-1); });
    connect(
        nextButton,
        &QToolButton::clicked,
        this,
        [this] { jumpRelative(1); });
    connect(
        zoomOutButton,
        &QToolButton::clicked,
        this,
        [this] {
            setCustomZoom(
                view_ ? view_->zoomFactor() / 1.2 : 1.0);
        });
    connect(
        zoomInButton,
        &QToolButton::clicked,
        this,
        [this] {
            setCustomZoom(
                view_ ? view_->zoomFactor() * 1.2 : 1.0);
        });
    connect(
        fitWidthButton,
        &QToolButton::clicked,
        this,
        [this] {
            if (view_)
                view_->setZoomMode(QPdfView::ZoomMode::FitToWidth);
        });
    connect(
        fitPageButton,
        &QToolButton::clicked,
        this,
        [this] {
            if (view_)
                view_->setZoomMode(QPdfView::ZoomMode::FitInView);
        });
    connect(
        view_->pageNavigator(),
        &QPdfPageNavigator::currentPageChanged,
        this,
        [this] { updatePageLabel(); });
}

QWidget* QtPdfBackend::widget()
{
    return container_;
}

bool QtPdfBackend::loadDocument(
    const QString& pdfPath,
    bool preserveViewState,
    QString& errorMessage)
{
    if (!container_ || !view_) {
        errorMessage = "The native PDF widget is no longer available.";
        return false;
    }

    const PdfViewState previousState =
        preserveViewState ? captureViewState() : PdfViewState{};

    auto* candidate = new QPdfDocument(container_);
    const QPdfDocument::Error error = candidate->load(pdfPath);

    if (error != QPdfDocument::Error::None) {
        errorMessage =
            QString("Unable to load cached PDF: %1.")
                .arg(pdfErrorText(error));
        candidate->deleteLater();
        return false;
    }

    QVector<QSizeF> candidatePageSizes;
    candidatePageSizes.reserve(candidate->pageCount());
    for (int page = 0; page < candidate->pageCount(); ++page)
        candidatePageSizes.append(candidate->pagePointSize(page));

    QPdfDocument* previousDocument = document_;
    document_ = candidate;
    pagePointSizes_ = std::move(candidatePageSizes);
    view_->setDocument(candidate);

    connect(
        candidate,
        &QPdfDocument::pageCountChanged,
        this,
        [this] { updatePageLabel(); });

    if (previousDocument)
        previousDocument->deleteLater();

    if (previousState.valid) {
        QTimer::singleShot(
            0,
            container_.data(),
            [this, previousState] {
                restoreViewState(previousState);
            });
    }

    updatePageLabel();
    errorMessage.clear();
    return true;
}

void QtPdfBackend::clear()
{
    if (view_)
        view_->setDocument(nullptr);

    if (document_)
        document_->deleteLater();

    document_.clear();
    pagePointSizes_.clear();
    updatePageLabel();
}

bool QtPdfBackend::hasDocument() const noexcept
{
    return document_
        && document_->status() == QPdfDocument::Status::Ready;
}

void QtPdfBackend::navigateTo(const PdfPosition& position)
{
    if (!view_ || !hasDocument() || !position.isValid())
        return;

    if (position.pageIndex >= document_->pageCount())
        return;

    view_->pageNavigator()->jump(
        position.pageIndex,
        position.point,
        0.0);
}

void QtPdfBackend::setInverseSyncCallback(
    InverseSyncCallback callback)
{
    inverseSyncCallback_ = std::move(callback);
}

bool QtPdfBackend::eventFilter(QObject* watched, QEvent* event)
{
    if (view_
        && watched == view_->viewport()
        && event->type() == QEvent::MouseButtonDblClick) {
        const auto* mouseEvent =
            static_cast<QMouseEvent*>(event);

        if (mouseEvent->button() == Qt::LeftButton) {
            handleInverseSyncClick(mouseEvent->position());
            return true;
        }
    }

    return QObject::eventFilter(watched, event);
}

PdfViewState QtPdfBackend::captureViewState() const
{
    PdfViewState state;

    if (!view_ || !hasDocument())
        return state;

    state.pageIndex = view_->pageNavigator()->currentPage();
    state.horizontalScroll = view_->horizontalScrollBar()->value();
    state.verticalScroll = view_->verticalScrollBar()->value();
    state.zoomMode = static_cast<int>(view_->zoomMode());
    state.zoomFactor = view_->zoomFactor();
    state.valid = true;
    return state;
}

void QtPdfBackend::restoreViewState(const PdfViewState& state)
{
    if (!view_ || !hasDocument() || !state.valid)
        return;

    const auto zoomMode =
        static_cast<QPdfView::ZoomMode>(state.zoomMode);

    view_->setZoomFactor(state.zoomFactor);
    view_->setZoomMode(zoomMode);

    const int page = std::clamp(
        state.pageIndex,
        0,
        std::max(0, document_->pageCount() - 1));

    view_->pageNavigator()->jump(page, QPointF(), 0.0);

    QTimer::singleShot(
        0,
        container_.data(),
        [this, state] {
            if (!view_)
                return;

            view_->horizontalScrollBar()->setValue(
                state.horizontalScroll);
            view_->verticalScrollBar()->setValue(
                state.verticalScroll);
        });
}

void QtPdfBackend::updatePageLabel()
{
    if (!pageLabel_)
        return;

    if (!view_ || !hasDocument() || document_->pageCount() <= 0) {
        pageLabel_->setText("No PDF");
        return;
    }

    pageLabel_->setText(
        QString("%1 / %2")
            .arg(view_->pageNavigator()->currentPage() + 1)
            .arg(document_->pageCount()));
}

void QtPdfBackend::jumpRelative(int pageDelta)
{
    if (!view_ || !hasDocument())
        return;

    const int nextPage = std::clamp(
        view_->pageNavigator()->currentPage() + pageDelta,
        0,
        std::max(0, document_->pageCount() - 1));

    view_->pageNavigator()->jump(nextPage, QPointF(), 0.0);
}

void QtPdfBackend::setCustomZoom(qreal factor)
{
    if (!view_)
        return;

    view_->setZoomMode(QPdfView::ZoomMode::Custom);
    view_->setZoomFactor(std::clamp(factor, 0.1, 8.0));
}

void QtPdfBackend::handleInverseSyncClick(
    const QPointF& viewportPoint)
{
    if (!view_ || !hasDocument() || !inverseSyncCallback_)
        return;

    QElapsedTimer mappingTimer;
    mappingTimer.start();

    PdfPageLayoutInput input;
    input.pagePointSizes = pagePointSizes_;

    input.viewportSize = view_->viewport()->size();
    input.documentMargins = view_->documentMargins();
    input.pageSpacing = view_->pageSpacing();
    input.currentPage = view_->pageNavigator()->currentPage();
    input.horizontalScroll = view_->horizontalScrollBar()->value();
    input.verticalScroll = view_->verticalScrollBar()->value();
    input.zoomFactor = view_->zoomFactor();

    const QScreen* screen = QGuiApplication::primaryScreen();
    input.screenResolution =
        screen ? screen->logicalDotsPerInch() / 72.0 : 1.0;

    input.pageMode =
        view_->pageMode() == QPdfView::PageMode::MultiPage
            ? PdfLayoutPageMode::MultiPage
            : PdfLayoutPageMode::SinglePage;

    switch (view_->zoomMode()) {
    case QPdfView::ZoomMode::Custom:
        input.zoomMode = PdfLayoutZoomMode::Custom;
        break;
    case QPdfView::ZoomMode::FitToWidth:
        input.zoomMode = PdfLayoutZoomMode::FitToWidth;
        break;
    case QPdfView::ZoomMode::FitInView:
        input.zoomMode = PdfLayoutZoomMode::FitInView;
        break;
    }

    const std::optional<PdfPosition> result =
        PdfPageLayoutMapper::mapViewportPoint(
            input,
            viewportPoint);

    if (!result.has_value()) {
#if !defined(NDEBUG)
        qInfo().noquote()
            << QString(
                   "PDF_HIT_METRIC outcome=no_page "
                   "viewport=(%1,%2) scroll=(%3,%4) map_us=%5")
                   .arg(viewportPoint.x(), 0, 'f', 2)
                   .arg(viewportPoint.y(), 0, 'f', 2)
                   .arg(input.horizontalScroll)
                   .arg(input.verticalScroll)
                   .arg(mappingTimer.nsecsElapsed() / 1000);
#endif
        return;
    }

#if !defined(NDEBUG)
    qInfo().noquote()
        << QString(
               "PDF_HIT_METRIC outcome=mapped "
               "viewport=(%1,%2) scroll=(%3,%4) "
               "page=%5 pdf=(%6,%7) dpi_scale=%8 map_us=%9")
               .arg(viewportPoint.x(), 0, 'f', 2)
               .arg(viewportPoint.y(), 0, 'f', 2)
               .arg(input.horizontalScroll)
               .arg(input.verticalScroll)
               .arg(result->pageIndex + 1)
               .arg(result->point.x(), 0, 'f', 4)
               .arg(result->point.y(), 0, 'f', 4)
               .arg(input.screenResolution, 0, 'f', 4)
               .arg(mappingTimer.nsecsElapsed() / 1000);
#endif

    inverseSyncCallback_(*result);
}
