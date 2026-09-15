#include "ui/RecoveryComparisonDialog.h"

#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>
#include <QWidget>

namespace
{
QWidget* createComparisonPane(
    const QString& label,
    const QString& text,
    QWidget* parent)
{
    auto* pane = new QWidget(parent);
    auto* layout = new QVBoxLayout(pane);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto* title = new QLabel(label, pane);

    auto* editor = new QPlainTextEdit(pane);
    editor->setReadOnly(true);
    editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    editor->setFont(
        QFontDatabase::systemFont(QFontDatabase::FixedFont));
    editor->setPlainText(text);

    layout->addWidget(title);
    layout->addWidget(editor, 1);

    return pane;
}
}

RecoveryComparisonDialog::RecoveryComparisonDialog(
    const QString& savedLabel,
    const QString& savedText,
    const QString& recoveredLabel,
    const QString& recoveredText,
    QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Compare Recovery");
    resize(1050, 680);
    setMinimumSize(760, 480);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    auto* explanation = new QLabel(
        "This comparison is read-only. "
        "Close returns to the recovery decision. "
        "Restore or Discard can be chosen directly here.",
        this);
    explanation->setWordWrap(true);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);
    splitter->addWidget(
        createComparisonPane(savedLabel, savedText, splitter));
    splitter->addWidget(
        createComparisonPane(
            recoveredLabel,
            recoveredText,
            splitter));
    splitter->setSizes({500, 500});

    auto* buttons = new QDialogButtonBox(this);

    auto* restoreButton =
        buttons->addButton("Restore", QDialogButtonBox::AcceptRole);
    auto* discardButton =
        buttons->addButton("Discard", QDialogButtonBox::DestructiveRole);
    auto* closeButton =
        buttons->addButton("Close", QDialogButtonBox::RejectRole);

    connect(
        restoreButton,
        &QPushButton::clicked,
        this,
        [this] {
            decision_ = Decision::Restore;
            accept();
        });

    connect(
        discardButton,
        &QPushButton::clicked,
        this,
        [this] {
            decision_ = Decision::Discard;
            accept();
        });

    connect(
        closeButton,
        &QPushButton::clicked,
        this,
        [this] {
            decision_ = Decision::Back;
            reject();
        });

    layout->addWidget(explanation);
    layout->addWidget(splitter, 1);
    layout->addWidget(buttons);
}

RecoveryComparisonDialog::Decision
RecoveryComparisonDialog::decision() const noexcept
{
    return decision_;
}
