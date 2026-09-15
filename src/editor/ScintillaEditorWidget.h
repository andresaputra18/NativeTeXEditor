#pragma once

#include "editor/IEditorBackend.h"

#include <QWidget>

class ScintillaEditBase;

class ScintillaEditorWidget final : public QWidget, public IEditorBackend
{
public:
    explicit ScintillaEditorWidget(QWidget* parent = nullptr);

    [[nodiscard]] QWidget* widget() override;
    [[nodiscard]] QString text() const override;

    void setText(const QString& text) override;
    void markSaved() override;
    [[nodiscard]] bool isModified() const override;
    void setModifiedStateCallback(ModifiedStateCallback callback) override;

    void undo() override;
    void redo() override;
    void cut() override;
    void copy() override;
    void paste() override;
    void selectAll() override;
    void focusEditor() override;
    [[nodiscard]] int currentLine() const override;
    [[nodiscard]] int currentColumn() const override;
    void goToLineColumn(int line, int column) override;

private:
    void configureEditor();
    void configureLatexLexer();

    ScintillaEditBase* editor_ = nullptr;
    ModifiedStateCallback modifiedStateCallback_;
};
