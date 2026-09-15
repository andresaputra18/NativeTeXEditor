#include "preview/PreviewService.h"

#include "services/IPdfBackend.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>

#include <utility>

PreviewService::PreviewService(
    const QString& applicationCacheDirectory,
    IPdfBackend& pdfBackend)
    : previewCacheRoot_(
          QDir(applicationCacheDirectory).filePath("preview"))
    , pdfBackend_(pdfBackend)
{
    QDir().mkpath(previewCacheRoot_);
}

PreviewService::~PreviewService()
{
    clear();
}

QWidget* PreviewService::widget()
{
    return pdfBackend_.widget();
}

bool PreviewService::activateSuccessfulBuild(
    const BuildResult& result,
    QString& errorMessage)
{
    if (!result.success) {
        errorMessage =
            "Only a successful build can create a preview generation.";
        return false;
    }

    const QFileInfo sourcePdf(result.pdfPath);
    if (!sourcePdf.isFile() || sourcePdf.size() <= 0) {
        errorMessage =
            QString("The successful build did not produce a readable PDF: %1")
                .arg(QDir::toNativeSeparators(result.pdfPath));
        return false;
    }

    PreviewGeneration candidate;
    candidate.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    candidate.buildTargetPath = normalizedPath(result.targetPath);
    candidate.originalPdfPath = normalizedPath(result.pdfPath);
    candidate.originalBuildDirectory = sourcePdf.absolutePath();
    candidate.cacheDirectory =
        QDir(previewCacheRoot_).filePath(candidate.id);
    candidate.cachedPdfPath =
        QDir(candidate.cacheDirectory).filePath("document.pdf");

    if (!QDir().mkpath(candidate.cacheDirectory)) {
        errorMessage =
            QString("Unable to create PDF preview cache directory: %1")
                .arg(QDir::toNativeSeparators(candidate.cacheDirectory));
        return false;
    }

    if (!copyGenerationFile(
            result.pdfPath,
            candidate.cachedPdfPath,
            errorMessage)) {
        removeGenerationDirectory(candidate.cacheDirectory);
        return false;
    }

    const QString sourceSyncTeX = matchingSyncTeXPath(result.pdfPath);
    if (!sourceSyncTeX.isEmpty()) {
        const QString suffix =
            sourceSyncTeX.endsWith(".gz", Qt::CaseInsensitive)
                ? QString("document.synctex.gz")
                : QString("document.synctex");

        candidate.cachedSyncTeXPath =
            QDir(candidate.cacheDirectory).filePath(suffix);

        if (!copyGenerationFile(
                sourceSyncTeX,
                candidate.cachedSyncTeXPath,
                errorMessage)) {
            removeGenerationDirectory(candidate.cacheDirectory);
            return false;
        }
    }

    const bool preserveViewState =
        currentGeneration_.has_value()
        && QString::compare(
               currentGeneration_->buildTargetPath,
               candidate.buildTargetPath,
               Qt::CaseInsensitive) == 0;

    if (!pdfBackend_.loadDocument(
            candidate.cachedPdfPath,
            preserveViewState,
            errorMessage)) {
        removeGenerationDirectory(candidate.cacheDirectory);
        return false;
    }

    const QString obsoleteDirectory =
        currentGeneration_.has_value()
            ? currentGeneration_->cacheDirectory
            : QString();

    currentGeneration_ = candidate;

    if (!obsoleteDirectory.isEmpty()
        && QString::compare(
               obsoleteDirectory,
               candidate.cacheDirectory,
               Qt::CaseInsensitive) != 0) {
        removeGenerationDirectory(obsoleteDirectory);
    }

    errorMessage.clear();
    return true;
}

void PreviewService::clear()
{
    pdfBackend_.clear();

    if (currentGeneration_.has_value())
        removeGenerationDirectory(currentGeneration_->cacheDirectory);

    currentGeneration_.reset();
}

void PreviewService::clearIfTargetChanged(const QString& targetPath)
{
    if (!currentGeneration_.has_value())
        return;

    if (targetPath.isEmpty()
        || QString::compare(
               currentGeneration_->buildTargetPath,
               normalizedPath(targetPath),
               Qt::CaseInsensitive) != 0) {
        clear();
    }
}

std::optional<PreviewGeneration> PreviewService::currentGeneration() const
{
    return currentGeneration_;
}

void PreviewService::navigateTo(const PdfPosition& position)
{
    pdfBackend_.navigateTo(position);
}

void PreviewService::setInverseSyncCallback(
    InverseSyncCallback callback)
{
    pdfBackend_.setInverseSyncCallback(std::move(callback));
}

QString PreviewService::matchingSyncTeXPath(const QString& pdfPath) const
{
    const QFileInfo pdfInfo(pdfPath);
    const QString basePath =
        QDir(pdfInfo.absolutePath()).filePath(pdfInfo.completeBaseName());

    const QString compressedPath = basePath + ".synctex.gz";
    if (QFileInfo(compressedPath).isFile())
        return compressedPath;

    const QString plainPath = basePath + ".synctex";
    if (QFileInfo(plainPath).isFile())
        return plainPath;

    return {};
}

bool PreviewService::copyGenerationFile(
    const QString& sourcePath,
    const QString& destinationPath,
    QString& errorMessage) const
{
    if (!QFile::copy(sourcePath, destinationPath)) {
        errorMessage =
            QString("Unable to copy preview generation file from %1 to %2.")
                .arg(
                    QDir::toNativeSeparators(sourcePath),
                    QDir::toNativeSeparators(destinationPath));
        return false;
    }

    if (!QFileInfo(destinationPath).isFile()
        || QFileInfo(destinationPath).size() <= 0) {
        errorMessage =
            QString("The cached preview generation file is empty: %1")
                .arg(QDir::toNativeSeparators(destinationPath));
        return false;
    }

    return true;
}

void PreviewService::removeGenerationDirectory(
    const QString& directoryPath) const
{
    const QString cleanRoot = QDir::cleanPath(previewCacheRoot_);
    const QString cleanDirectory = QDir::cleanPath(directoryPath);

    if (cleanDirectory.isEmpty()
        || cleanDirectory == cleanRoot
        || !QFileInfo(cleanDirectory).isDir()) {
        return;
    }

    const QString relative = QDir::fromNativeSeparators(
        QDir(cleanRoot).relativeFilePath(cleanDirectory));

    if (relative.isEmpty()
        || relative == "."
        || relative == ".."
        || relative.startsWith("../")
        || QFileInfo(relative).isAbsolute()) {
        return;
    }

    QDir(cleanDirectory).removeRecursively();
}

QString PreviewService::normalizedPath(const QString& path)
{
    if (path.isEmpty())
        return {};

    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    return QDir::cleanPath(
        canonical.isEmpty() ? info.absoluteFilePath() : canonical);
}
