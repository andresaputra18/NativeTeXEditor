#pragma once

#include <QDialog>

class QString;
class QWidget;

class RecoveryComparisonDialog final : public QDialog
{
public:
    enum class Decision
    {
        Back,
        Restore,
        Discard
    };

    RecoveryComparisonDialog(
        const QString& savedLabel,
        const QString& savedText,
        const QString& recoveredLabel,
        const QString& recoveredText,
        QWidget* parent = nullptr);

    [[nodiscard]] Decision decision() const noexcept;

private:
    Decision decision_ = Decision::Back;
};
