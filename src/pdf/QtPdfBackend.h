#pragma once

#include "services/IPdfBackend.h"

#include <QObject>
#include <QPointer>
#include <QSizeF>
#include <QVector>

class QLabel;
class QPdfDocument;
class QPdfView;
class QWidget;

class QtPdfBackend final : public QObject, public IPdfBackend
{
public:
    explicit QtPdfBackend(QObject* parent = nullptr);

    [[nodiscard]] QWidget* widget() override;
    [[nodiscard]] bool loadDocument(
        const QString& pdfPath,
        bool preserveViewState,
        QString& errorMessage) override;
    void clear() override;
    [[nodiscard]] bool hasDocument() const noexcept override;
    void navigateTo(const PdfPosition& position) override;
    void setInverseSyncCallback(
        InverseSyncCallback callback) override;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    [[nodiscard]] PdfViewState captureViewState() const;
    void restoreViewState(const PdfViewState& state);
    void updatePageLabel();
    void jumpRelative(int pageDelta);
    void setCustomZoom(qreal factor);
    void handleInverseSyncClick(const QPointF& viewportPoint);

    QPointer<QWidget> container_;
    QPointer<QPdfView> view_;
    QPointer<QPdfDocument> document_;
    QPointer<QLabel> pageLabel_;
    QVector<QSizeF> pagePointSizes_;
    InverseSyncCallback inverseSyncCallback_;
};
