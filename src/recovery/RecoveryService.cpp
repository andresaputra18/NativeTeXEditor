#include "recovery/RecoveryService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>

namespace
{
constexpr int RecoveryFormatVersion = 1;
}

RecoveryService::RecoveryService(const QString& recoveryDirectory)
    : snapshotFilePath_(
          QDir(recoveryDirectory).filePath("active-recovery.json"))
{
}

bool RecoveryService::hasSnapshot() const
{
    return QFileInfo::exists(snapshotFilePath_);
}

bool RecoveryService::writeSnapshot(
    const QString& originalFilePath,
    const QString& text,
    QString& errorMessage)
{
    QJsonObject object;
    object.insert("version", RecoveryFormatVersion);
    object.insert("originalFilePath", originalFilePath);
    object.insert("text", text);
    object.insert(
        "timestampUtc",
        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));

    const QByteArray payload =
        QJsonDocument(object).toJson(QJsonDocument::Compact);

    QSaveFile file(snapshotFilePath_);

    if (!file.open(QIODevice::WriteOnly)) {
        errorMessage =
            QString("Unable to create recovery snapshot:\n%1\n\n%2")
                .arg(snapshotFilePath_, file.errorString());
        return false;
    }

    const qint64 written = file.write(payload);

    if (written != payload.size()) {
        errorMessage =
            QString("Unable to write the complete recovery snapshot:\n%1\n\n%2")
                .arg(snapshotFilePath_, file.errorString());
        file.cancelWriting();
        return false;
    }

    if (!file.commit()) {
        errorMessage =
            QString("Unable to commit the recovery snapshot atomically:\n%1\n\n%2")
                .arg(snapshotFilePath_, file.errorString());
        return false;
    }

    errorMessage.clear();
    return true;
}

std::optional<RecoverySnapshot> RecoveryService::loadSnapshot(
    QString& errorMessage) const
{
    errorMessage.clear();

    if (!hasSnapshot())
        return std::nullopt;

    QFile file(snapshotFilePath_);

    if (!file.open(QIODevice::ReadOnly)) {
        errorMessage =
            QString("Unable to read recovery snapshot:\n%1\n\n%2")
                .arg(snapshotFilePath_, file.errorString());
        return std::nullopt;
    }

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(file.readAll(), &parseError);

    if (parseError.error != QJsonParseError::NoError
        || !document.isObject()) {
        errorMessage =
            QString("Recovery snapshot is invalid or corrupted:\n%1")
                .arg(snapshotFilePath_);
        return std::nullopt;
    }

    const QJsonObject object = document.object();

    if (object.value("version").toInt(-1) != RecoveryFormatVersion
        || !object.value("originalFilePath").isString()
        || !object.value("text").isString()
        || !object.value("timestampUtc").isString()) {
        errorMessage =
            QString("Recovery snapshot has an unsupported format:\n%1")
                .arg(snapshotFilePath_);
        return std::nullopt;
    }

    const QDateTime timestamp =
        QDateTime::fromString(
            object.value("timestampUtc").toString(),
            Qt::ISODateWithMs);

    if (!timestamp.isValid()) {
        errorMessage =
            QString("Recovery snapshot contains an invalid timestamp:\n%1")
                .arg(snapshotFilePath_);
        return std::nullopt;
    }

    RecoverySnapshot snapshot;
    snapshot.originalFilePath =
        object.value("originalFilePath").toString();
    snapshot.text = object.value("text").toString();
    snapshot.timestampUtc = timestamp.toUTC();

    return snapshot;
}

bool RecoveryService::discardSnapshot(QString& errorMessage)
{
    errorMessage.clear();

    if (!hasSnapshot())
        return true;

    if (!QFile::remove(snapshotFilePath_)) {
        errorMessage =
            QString("Unable to remove recovery snapshot:\n%1")
                .arg(snapshotFilePath_);
        return false;
    }

    return true;
}
