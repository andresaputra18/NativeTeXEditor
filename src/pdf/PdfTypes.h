#pragma once

#include <QPointF>

struct PdfPosition final
{
    int pageIndex = -1;
    QPointF point;

    [[nodiscard]] bool isValid() const noexcept
    {
        return pageIndex >= 0
            && point.x() >= 0.0
            && point.y() >= 0.0;
    }
};

struct PdfViewState final
{
    int pageIndex = 0;
    int horizontalScroll = 0;
    int verticalScroll = 0;
    int zoomMode = 0;
    qreal zoomFactor = 1.0;
    bool valid = false;
};
