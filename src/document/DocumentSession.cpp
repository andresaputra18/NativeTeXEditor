#include "document/DocumentSession.h"

#include <QFileInfo>

DocumentSession::DocumentSession() = default;

void DocumentSession::resetUntitled()
{
    filePath_.clear();
    dirty_ = false;
}

void DocumentSession::setFilePath(const QString& filePath)
{
    filePath_ = QFileInfo(filePath).absoluteFilePath();
}

const QString& DocumentSession::filePath() const noexcept
{
    return filePath_;
}

QString DocumentSession::displayName() const
{
    if (filePath_.isEmpty())
        return "Untitled";

    return QFileInfo(filePath_).fileName();
}

bool DocumentSession::hasFilePath() const noexcept
{
    return !filePath_.isEmpty();
}

void DocumentSession::setDirty(bool dirty) noexcept
{
    dirty_ = dirty;
}

bool DocumentSession::isDirty() const noexcept
{
    return dirty_;
}
