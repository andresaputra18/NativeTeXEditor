#pragma once

#include "pdf/PdfTypes.h"

#include <QMargins>
#include <QPointF>
#include <QSize>
#include <QSizeF>
#include <QVector>

#include <optional>

enum class PdfLayoutPageMode
{
    SinglePage,
    MultiPage
};

enum class PdfLayoutZoomMode
{
    Custom,
    FitToWidth,
    FitInView
};

struct PdfPageLayoutInput final
{
    QVector<QSizeF> pagePointSizes;
    QSize viewportSize;
    QMargins documentMargins;
    int pageSpacing = 3;
    int currentPage = 0;
    int horizontalScroll = 0;
    int verticalScroll = 0;
    qreal screenResolution = 1.0;
    qreal zoomFactor = 1.0;
    PdfLayoutPageMode pageMode = PdfLayoutPageMode::MultiPage;
    PdfLayoutZoomMode zoomMode = PdfLayoutZoomMode::Custom;
};

class PdfPageLayoutMapper final
{
public:
    [[nodiscard]] static std::optional<PdfPosition> mapViewportPoint(
        const PdfPageLayoutInput& input,
        const QPointF& viewportPoint);
};
