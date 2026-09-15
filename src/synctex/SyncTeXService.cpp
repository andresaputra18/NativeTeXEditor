#include "synctex/SyncTeXService.h"

#include "services/IProcessSupervisor.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QStringList>

#include <algorithm>
#include <utility>

namespace
{
constexpr int SyncTeXTimeoutMilliseconds = 5000;

QString numericArgument(qreal value)
{
    return QLocale::c().toString(value, 'f', 6);
}

bool parseInteger(const QString& value, int& result)
{
    bool ok = false;
    const int parsed = value.trimmed().toInt(&ok);
    if (ok)
        result = parsed;
    return ok;
}

bool parseReal(const QString& value, qreal& result)
{
    bool ok = false;
    const double parsed = QLocale::c().toDouble(value.trimmed(), &ok);
    if (ok)
        result = parsed;
    return ok;
}
}

SyncTeXService::SyncTeXService(IProcessSupervisor& processSupervisor)
    : processSupervisor_(processSupervisor)
{
}

SyncTeXService::~SyncTeXService()
{
    lifetimeToken_.reset();
    activeCallback_ = {};
    processSupervisor_.cancel();
}

bool SyncTeXService::forward(
    const SyncTeXRequestContext& context,
    const SourcePosition& source,
    ResultCallback callback)
{
    if (!source.isValid())
        return false;

    const QString nativeSourcePath =
        QDir::toNativeSeparators(
            QFileInfo(source.filePath).absoluteFilePath());

    const QString input =
        QString("%1:%2:%3")
            .arg(source.line)
            .arg(std::max(0, source.column))
            .arg(nativeSourcePath);

    return start(
        Direction::Forward,
        context,
        QStringList{
            "view",
            "-i",
            input,
            "-o",
            QDir::toNativeSeparators(
                context.generation.cachedPdfPath)
        },
        std::move(callback));
}

bool SyncTeXService::inverse(
    const SyncTeXRequestContext& context,
    const PdfPosition& pdf,
    ResultCallback callback)
{
    if (!pdf.isValid())
        return false;

    const QString output =
        QString("%1:%2:%3:%4")
            .arg(pdf.pageIndex + 1)
            .arg(numericArgument(pdf.point.x()))
            .arg(numericArgument(pdf.point.y()))
            .arg(QDir::toNativeSeparators(
                context.generation.cachedPdfPath));

    return start(
        Direction::Inverse,
        context,
        QStringList{
            "edit",
            "-o",
            output
        },
        std::move(callback));
}

void SyncTeXService::cancel()
{
    processSupervisor_.cancel();
}

bool SyncTeXService::isRunning() const noexcept
{
    return processSupervisor_.isRunning();
}

bool SyncTeXService::start(
    Direction direction,
    const SyncTeXRequestContext& context,
    const QStringList& arguments,
    ResultCallback callback)
{
    if (processSupervisor_.isRunning()
        || context.executablePath.isEmpty()
        || !QFileInfo(context.executablePath).isFile()
        || !QFileInfo(context.generation.cachedPdfPath).isFile()
        || !context.generation.hasSyncTeX()
        || !QFileInfo(context.generation.cachedSyncTeXPath).isFile()) {
        return false;
    }

    ProcessRequest request;
    request.program = context.executablePath;
    request.arguments = arguments;
    request.workingDirectory = context.generation.cacheDirectory;
    request.timeoutMilliseconds = SyncTeXTimeoutMilliseconds;

    activeDirection_ = direction;
    activeGenerationId_ = context.generation.id;
    activeCallback_ = std::move(callback);
    elapsedTimer_.restart();

#if !defined(NDEBUG)
    qInfo().noquote()
        << QString(
               "SYNC_METRIC_BEGIN direction=%1 generation=%2 "
               "program=\"%3\" arguments=\"%4\"")
               .arg(
                   directionName(direction),
                   activeGenerationId_,
                   QDir::toNativeSeparators(request.program),
                   request.arguments.join(" | "));
#endif

    const std::weak_ptr<int> lifetime = lifetimeToken_;
    const bool accepted = processSupervisor_.start(
        request,
        {},
        [this, lifetime](const ProcessResult& result) {
            if (lifetime.expired())
                return;
            finish(result);
        });

    if (!accepted) {
        activeCallback_ = {};
        activeGenerationId_.clear();
    }

    return accepted;
}

void SyncTeXService::finish(const ProcessResult& processResult)
{
    const qint64 totalElapsed =
        elapsedTimer_.isValid() ? elapsedTimer_.elapsed() : 0;

    SyncTeXResult result =
        activeDirection_ == Direction::Forward
            ? parseForward(processResult)
            : parseInverse(processResult);
    result.elapsedMilliseconds = totalElapsed;

#if !defined(NDEBUG)
    qInfo().noquote()
        << QString(
               "SYNC_METRIC_END direction=%1 generation=%2 "
               "outcome=%3 elapsed_ms=%4 process_ms=%5 exit_code=%6")
               .arg(
                   directionName(activeDirection_),
                   activeGenerationId_,
                   result.success ? QString("success") : QString("failed"))
               .arg(totalElapsed)
               .arg(processResult.elapsedMilliseconds)
               .arg(processResult.exitCode);
    qInfo().noquote()
        << QString(
               "SYNC_COMMAND_OUTPUT_BEGIN\n%1\n"
               "SYNC_COMMAND_OUTPUT_END")
               .arg(processResult.output);

    if (result.success
        && activeDirection_ == Direction::Forward) {
        qInfo().noquote()
            << QString(
                   "SYNC_PARSED_RESULT direction=forward "
                   "page=%1 pdf=(%2,%3)")
                   .arg(result.pdfPosition.pageIndex + 1)
                   .arg(result.pdfPosition.point.x(), 0, 'f', 6)
                   .arg(result.pdfPosition.point.y(), 0, 'f', 6);
    } else if (result.success) {
        qInfo().noquote()
            << QString(
                   "SYNC_PARSED_RESULT direction=inverse "
                   "source=\"%1\" line=%2 column=%3")
                   .arg(
                       QDir::toNativeSeparators(
                           result.sourcePosition.filePath))
                   .arg(result.sourcePosition.line)
                   .arg(result.sourcePosition.column);
    }
#endif

    auto callback = std::move(activeCallback_);
    activeGenerationId_.clear();

    if (callback)
        callback(result);
}

SyncTeXResult SyncTeXService::parseForward(
    const ProcessResult& processResult) const
{
    SyncTeXResult result;
    result.rawOutput = processResult.output;

    int page = 0;
    qreal x = 0.0;
    qreal y = 0.0;
    bool havePage = false;
    bool haveX = false;
    bool haveY = false;

    const QStringList lines =
        processResult.output.split('\n', Qt::KeepEmptyParts);

    for (const QString& originalLine : lines) {
        const QString line = originalLine.trimmed();

        if (line.startsWith("SyncTeX result begin", Qt::CaseInsensitive)) {
            page = 0;
            x = 0.0;
            y = 0.0;
            havePage = false;
            haveX = false;
            haveY = false;
            continue;
        }

        if (line.startsWith("Page:", Qt::CaseInsensitive)) {
            havePage = parseInteger(valueAfterPrefix(line, "Page:"), page);
        } else if (line.startsWith("x:", Qt::CaseInsensitive)) {
            haveX = parseReal(valueAfterPrefix(line, "x:"), x);
        } else if (line.startsWith("y:", Qt::CaseInsensitive)) {
            haveY = parseReal(valueAfterPrefix(line, "y:"), y);
        } else if (line.startsWith("h:", Qt::CaseInsensitive) && !haveX) {
            haveX = parseReal(valueAfterPrefix(line, "h:"), x);
        } else if (line.startsWith("v:", Qt::CaseInsensitive) && !haveY) {
            haveY = parseReal(valueAfterPrefix(line, "v:"), y);
        }

        if (havePage && haveX && haveY && page > 0) {
            result.pdfPosition.pageIndex = page - 1;
            result.pdfPosition.point = QPointF(x, y);
            result.success = true;
            return result;
        }
    }

    if (!processResult.errorMessage.isEmpty()) {
        result.errorMessage = processResult.errorMessage;
    } else if (processResult.timedOut) {
        result.errorMessage = "SyncTeX query timed out.";
    } else if (processResult.cancelled) {
        result.errorMessage = "SyncTeX query was cancelled.";
    } else {
        result.errorMessage =
            "SyncTeX returned no forward source-to-PDF match.";
    }

    return result;
}

SyncTeXResult SyncTeXService::parseInverse(
    const ProcessResult& processResult) const
{
    SyncTeXResult result;
    result.rawOutput = processResult.output;

    SourcePosition candidate;
    bool haveInput = false;
    bool haveLine = false;

    const QStringList lines =
        processResult.output.split('\n', Qt::KeepEmptyParts);

    for (const QString& originalLine : lines) {
        const QString line = originalLine.trimmed();

        if (line.startsWith("SyncTeX result begin", Qt::CaseInsensitive)) {
            if (haveInput && haveLine && candidate.line > 0)
                break;

            candidate = {};
            haveInput = false;
            haveLine = false;
            continue;
        }

        if (line.startsWith("SyncTeX result end", Qt::CaseInsensitive))
            break;

        if (line.startsWith("Input:", Qt::CaseInsensitive)) {
            candidate.filePath = valueAfterPrefix(line, "Input:");
            if (candidate.filePath.size() >= 2
                && candidate.filePath.startsWith('"')
                && candidate.filePath.endsWith('"')) {
                candidate.filePath = candidate.filePath.mid(
                    1,
                    candidate.filePath.size() - 2);
            }
            haveInput = !candidate.filePath.isEmpty();
        } else if (line.startsWith("Line:", Qt::CaseInsensitive)) {
            haveLine = parseInteger(
                valueAfterPrefix(line, "Line:"),
                candidate.line);
        } else if (line.startsWith("Column:", Qt::CaseInsensitive)) {
            static_cast<void>(parseInteger(
                valueAfterPrefix(line, "Column:"),
                candidate.column));
        }

    }

    if (haveInput && haveLine && candidate.line > 0) {
        if (candidate.column <= 0)
            candidate.column = 1;

        result.sourcePosition = candidate;
        result.success = true;
        return result;
    }

    if (!processResult.errorMessage.isEmpty()) {
        result.errorMessage = processResult.errorMessage;
    } else if (processResult.timedOut) {
        result.errorMessage = "SyncTeX query timed out.";
    } else if (processResult.cancelled) {
        result.errorMessage = "SyncTeX query was cancelled.";
    } else {
        result.errorMessage =
            "SyncTeX returned no inverse PDF-to-source match.";
    }

    return result;
}

QString SyncTeXService::valueAfterPrefix(
    const QString& line,
    const QString& prefix)
{
    return line.mid(prefix.size()).trimmed();
}

QString SyncTeXService::directionName(Direction direction)
{
    return direction == Direction::Forward
        ? QString("forward")
        : QString("inverse");
}
