#include "pdf/PdfPageLayoutMapper.h"

#include <QRect>

#include <algorithm>

namespace
{
struct PageGeometry final
{
    int pageIndex = -1;
    QRect rectangle;
    qreal pixelsPerPoint = 1.0;
};

QSize scaledPageSize(
    const PdfPageLayoutInput& input,
    const QSizeF& pointSize,
    qreal& pixelsPerPoint)
{
    const QSize basePixelSize =
        (pointSize * input.screenResolution).toSize();

    if (basePixelSize.isEmpty()) {
        pixelsPerPoint = 0.0;
        return {};
    }

    if (input.zoomMode == PdfLayoutZoomMode::Custom) {
        pixelsPerPoint =
            input.screenResolution * input.zoomFactor;
        return (pointSize * pixelsPerPoint).toSize();
    }

    if (input.zoomMode == PdfLayoutZoomMode::FitToWidth) {
        const int availableWidth =
            std::max(
                1,
                input.viewportSize.width()
                    - input.documentMargins.left()
                    - input.documentMargins.right());

        const qreal pageScale =
            static_cast<qreal>(availableWidth)
            / static_cast<qreal>(basePixelSize.width());

        pixelsPerPoint =
            input.screenResolution * pageScale;

        QSize result = basePixelSize;
        result *= pageScale;
        return result;
    }

    const QSize availableSize =
        input.viewportSize
        + QSize(
            -input.documentMargins.left()
                - input.documentMargins.right(),
            -input.pageSpacing);

    const QSize fitted = basePixelSize.scaled(
        availableSize.expandedTo(QSize(1, 1)),
        Qt::KeepAspectRatio);

    const qreal pageScale =
        static_cast<qreal>(fitted.width())
        / static_cast<qreal>(basePixelSize.width());

    pixelsPerPoint =
        input.screenResolution * pageScale;
    return fitted;
}
}

std::optional<PdfPosition> PdfPageLayoutMapper::mapViewportPoint(
    const PdfPageLayoutInput& input,
    const QPointF& viewportPoint)
{
    if (input.pagePointSizes.isEmpty()
        || input.viewportSize.isEmpty()
        || input.screenResolution <= 0.0
        || input.zoomFactor <= 0.0) {
        return std::nullopt;
    }

    const int pageCount =
        static_cast<int>(input.pagePointSizes.size());

    const int startPage =
        input.pageMode == PdfLayoutPageMode::SinglePage
            ? std::clamp(
                  input.currentPage,
                  0,
                  pageCount - 1)
            : 0;

    const int endPage =
        input.pageMode == PdfLayoutPageMode::SinglePage
            ? startPage + 1
            : pageCount;

    QVector<PageGeometry> pages;
    pages.reserve(endPage - startPage);

    int totalWidth = 0;

    for (int page = startPage; page < endPage; ++page) {
        qreal pixelsPerPoint = 0.0;
        const QSize pageSize = scaledPageSize(
            input,
            input.pagePointSizes.at(page),
            pixelsPerPoint);

        if (pageSize.isEmpty() || pixelsPerPoint <= 0.0)
            continue;

        totalWidth = std::max(totalWidth, pageSize.width());

        PageGeometry geometry;
        geometry.pageIndex = page;
        geometry.rectangle = QRect(QPoint(0, 0), pageSize);
        geometry.pixelsPerPoint = pixelsPerPoint;
        pages.append(geometry);
    }

    totalWidth +=
        input.documentMargins.left()
        + input.documentMargins.right();

    int pageY = input.documentMargins.top();

    for (PageGeometry& page : pages) {
        const int pageX =
            (std::max(totalWidth, input.viewportSize.width())
             - page.rectangle.width())
            / 2;

        page.rectangle.moveTopLeft(QPoint(pageX, pageY));
        pageY += page.rectangle.height() + input.pageSpacing;
    }

    const QPointF documentPoint =
        viewportPoint
        + QPointF(
            input.horizontalScroll,
            input.verticalScroll);

    for (const PageGeometry& page : pages) {
        if (!QRectF(page.rectangle).contains(documentPoint))
            continue;

        QPointF pdfPoint =
            (documentPoint - page.rectangle.topLeft())
            / page.pixelsPerPoint;

        const QSizeF pointSize =
            input.pagePointSizes.at(page.pageIndex);

        pdfPoint.setX(
            std::clamp(pdfPoint.x(), 0.0, pointSize.width()));
        pdfPoint.setY(
            std::clamp(pdfPoint.y(), 0.0, pointSize.height()));

        return PdfPosition{page.pageIndex, pdfPoint};
    }

    return std::nullopt;
}
