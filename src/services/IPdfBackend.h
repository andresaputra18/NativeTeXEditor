#pragma once

#include "pdf/PdfTypes.h"

#include <QString>

#include <functional>

class QWidget;

class IPdfBackend
{
public:
    using InverseSyncCallback =
        std::function<void(const PdfPosition&)>;

    virtual ~IPdfBackend() = default;

    [[nodiscard]] virtual QWidget* widget() = 0;
    [[nodiscard]] virtual bool loadDocument(
        const QString& pdfPath,
        bool preserveViewState,
        QString& errorMessage) = 0;
    virtual void clear() = 0;
    [[nodiscard]] virtual bool hasDocument() const noexcept = 0;
    virtual void navigateTo(const PdfPosition& position) = 0;
    virtual void setInverseSyncCallback(
        InverseSyncCallback callback) = 0;
};
