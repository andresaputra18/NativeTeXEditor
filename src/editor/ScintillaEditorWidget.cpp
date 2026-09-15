#include "editor/ScintillaEditorWidget.h"

#include "ILexer.h"
#include "Lexilla.h"
#include "SciLexer.h"
#include "Scintilla.h"
#include "ScintillaEditBase.h"

#include <QByteArray>
#include <QDebug>
#include <QFont>
#include <QLabel>
#include <QVBoxLayout>

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

namespace
{
constexpr std::intptr_t scintillaColour(int red, int green, int blue) noexcept
{
    return red | (green << 8) | (blue << 16);
}
}

ScintillaEditorWidget::ScintillaEditorWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* title = new QLabel("Editor", this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setContentsMargins(12, 8, 12, 8);

    editor_ = new ScintillaEditBase(this);

    layout->addWidget(title);
    layout->addWidget(editor_, 1);

    configureEditor();
    configureLatexLexer();

    connect(
        editor_,
        &ScintillaEditBase::savePointChanged,
        this,
        [this](bool modified) {
            if (modifiedStateCallback_)
                modifiedStateCallback_(modified);
        });

    setText({});
}

QWidget* ScintillaEditorWidget::widget()
{
    return this;
}

QString ScintillaEditorWidget::text() const
{
    const auto length = static_cast<int>(editor_->send(SCI_GETTEXTLENGTH));
    std::string buffer(static_cast<std::size_t>(length) + 1U, '\0');

    editor_->send(
        SCI_GETTEXT,
        static_cast<uptr_t>(buffer.size()),
        reinterpret_cast<sptr_t>(buffer.data()));

    return QString::fromUtf8(buffer.data(), length);
}

void ScintillaEditorWidget::setText(const QString& text)
{
    const QByteArray utf8 = text.toUtf8();

    editor_->sends(SCI_SETTEXT, 0, utf8.constData());
    editor_->send(SCI_EMPTYUNDOBUFFER);
    editor_->send(SCI_SETSAVEPOINT);
    editor_->send(SCI_COLOURISE, 0, -1);
    editor_->send(SCI_GOTOPOS, 0);
}

void ScintillaEditorWidget::markSaved()
{
    editor_->send(SCI_SETSAVEPOINT);
}

bool ScintillaEditorWidget::isModified() const
{
    return editor_->send(SCI_GETMODIFY) != 0;
}

void ScintillaEditorWidget::setModifiedStateCallback(ModifiedStateCallback callback)
{
    modifiedStateCallback_ = std::move(callback);
}

void ScintillaEditorWidget::undo()
{
    editor_->send(SCI_UNDO);
}

void ScintillaEditorWidget::redo()
{
    editor_->send(SCI_REDO);
}

void ScintillaEditorWidget::cut()
{
    editor_->send(SCI_CUT);
}

void ScintillaEditorWidget::copy()
{
    editor_->send(SCI_COPY);
}

void ScintillaEditorWidget::paste()
{
    editor_->send(SCI_PASTE);
}

void ScintillaEditorWidget::selectAll()
{
    editor_->send(SCI_SELECTALL);
}

void ScintillaEditorWidget::focusEditor()
{
    editor_->setFocus(Qt::OtherFocusReason);
}

int ScintillaEditorWidget::currentLine() const
{
    const sptr_t position = editor_->send(SCI_GETCURRENTPOS);
    return static_cast<int>(editor_->send(
               SCI_LINEFROMPOSITION,
               static_cast<uptr_t>(position)))
        + 1;
}

int ScintillaEditorWidget::currentColumn() const
{
    const sptr_t position = editor_->send(SCI_GETCURRENTPOS);
    return static_cast<int>(editor_->send(
               SCI_GETCOLUMN,
               static_cast<uptr_t>(position)))
        + 1;
}

void ScintillaEditorWidget::goToLineColumn(int line, int column)
{
    const int zeroBasedLine = std::max(0, line - 1);
    const int zeroBasedColumn = std::max(0, column - 1);
    const sptr_t position = editor_->send(
        SCI_FINDCOLUMN,
        static_cast<uptr_t>(zeroBasedLine),
        static_cast<sptr_t>(zeroBasedColumn));

    editor_->send(SCI_GOTOPOS, static_cast<uptr_t>(position));
    editor_->send(SCI_SCROLLCARET);
    focusEditor();
}

void ScintillaEditorWidget::configureEditor()
{
    editor_->send(SCI_SETCODEPAGE, SC_CP_UTF8);
    editor_->send(SCI_SETUSETABS, 0);
    editor_->send(SCI_SETTABWIDTH, 4);
    editor_->send(SCI_SETINDENT, 4);
    editor_->send(SCI_SETWRAPMODE, SC_WRAP_NONE);
    editor_->send(SCI_SETSCROLLWIDTHTRACKING, 1);

    editor_->send(SCI_SETMARGINTYPEN, 0, SC_MARGIN_NUMBER);
    editor_->send(SCI_SETMARGINWIDTHN, 0, 48);

    editor_->sends(SCI_STYLESETFONT, STYLE_DEFAULT, "Cascadia Mono");
    editor_->send(SCI_STYLESETSIZE, STYLE_DEFAULT, 11);
    editor_->send(SCI_STYLESETFORE, STYLE_DEFAULT, scintillaColour(35, 38, 41));
    editor_->send(SCI_STYLESETBACK, STYLE_DEFAULT, scintillaColour(250, 250, 250));
    editor_->send(SCI_STYLECLEARALL);

    editor_->send(SCI_STYLESETFORE, STYLE_LINENUMBER, scintillaColour(115, 118, 122));
    editor_->send(SCI_STYLESETBACK, STYLE_LINENUMBER, scintillaColour(242, 243, 245));

    editor_->send(SCI_SETCARETLINEVISIBLE, 1);
    editor_->send(SCI_SETCARETLINEBACK, scintillaColour(232, 236, 242));
    editor_->send(SCI_SETCARETLINEBACKALPHA, 96);
}

void ScintillaEditorWidget::configureLatexLexer()
{
    Scintilla::ILexer5* lexer = CreateLexer("tex");

    if (!lexer) {
        qWarning() << "Lexilla failed to create the TeX lexer.";
        return;
    }

    // SCI_SETILEXER transfers ownership of the lexer to Scintilla.
    editor_->send(
        SCI_SETILEXER,
        0,
        reinterpret_cast<sptr_t>(lexer));

    editor_->send(SCI_STYLESETFORE, SCE_TEX_DEFAULT, scintillaColour(50, 52, 55));
    editor_->send(SCI_STYLESETFORE, SCE_TEX_SPECIAL, scintillaColour(150, 80, 30));
    editor_->send(SCI_STYLESETFORE, SCE_TEX_GROUP, scintillaColour(135, 45, 70));
    editor_->send(SCI_STYLESETFORE, SCE_TEX_SYMBOL, scintillaColour(95, 70, 145));
    editor_->send(SCI_STYLESETFORE, SCE_TEX_COMMAND, scintillaColour(20, 95, 165));
    editor_->send(SCI_STYLESETFORE, SCE_TEX_TEXT, scintillaColour(35, 38, 41));

    editor_->send(SCI_COLOURISE, 0, -1);
}
