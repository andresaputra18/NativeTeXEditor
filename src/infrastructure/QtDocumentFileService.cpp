#include "infrastructure/QtDocumentFileService.h"

#include <QFile>
#include <QSaveFile>
#include <QStringDecoder>

bool QtDocumentFileService::readUtf8(
    const QString& filePath,
    QString& text,
    QString& errorMessage) const
{
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly)) {
        errorMessage =
            QString("Unable to open file for reading:\n%1\n\n%2")
                .arg(filePath, file.errorString());
        return false;
    }

    QByteArray bytes = file.readAll();

    if (bytes.startsWith("\xEF\xBB\xBF"))
        bytes.remove(0, 3);

    QStringDecoder decoder(QStringDecoder::Utf8);
    const QString decoded = decoder.decode(bytes);

    if (decoder.hasError()) {
        errorMessage =
            QString("The file is not valid UTF-8:\n%1")
                .arg(filePath);
        return false;
    }

    text = decoded;
    return true;
}

bool QtDocumentFileService::writeUtf8Atomic(
    const QString& filePath,
    const QString& text,
    QString& errorMessage) const
{
    QSaveFile file(filePath);

    if (!file.open(QIODevice::WriteOnly)) {
        errorMessage =
            QString("Unable to open file for saving:\n%1\n\n%2")
                .arg(filePath, file.errorString());
        return false;
    }

    const QByteArray utf8 = text.toUtf8();
    const qint64 written = file.write(utf8);

    if (written != utf8.size()) {
        errorMessage =
            QString("Unable to write the complete file:\n%1\n\n%2")
                .arg(filePath, file.errorString());
        file.cancelWriting();
        return false;
    }

    if (!file.commit()) {
        errorMessage =
            QString("Unable to commit the saved file atomically:\n%1\n\n%2")
                .arg(filePath, file.errorString());
        return false;
    }

    return true;
}
