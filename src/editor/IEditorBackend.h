#pragma once

#include <QString>

#include <functional>

class QWidget;

class IEditorBackend
{
public:
    using ModifiedStateCallback = std::function<void(bool modified)>;

    virtual ~IEditorBackend() = default;

    [[nodiscard]] virtual QWidget* widget() = 0;
    [[nodiscard]] virtual QString text() const = 0;

    virtual void setText(const QString& text) = 0;
    virtual void markSaved() = 0;
    [[nodiscard]] virtual bool isModified() const = 0;
    virtual void setModifiedStateCallback(ModifiedStateCallback callback) = 0;

    virtual void undo() = 0;
    virtual void redo() = 0;
    virtual void cut() = 0;
    virtual void copy() = 0;
    virtual void paste() = 0;
    virtual void selectAll() = 0;
    virtual void focusEditor() = 0;
    [[nodiscard]] virtual int currentLine() const = 0;
    [[nodiscard]] virtual int currentColumn() const = 0;
    virtual void goToLineColumn(int line, int column) = 0;
};
