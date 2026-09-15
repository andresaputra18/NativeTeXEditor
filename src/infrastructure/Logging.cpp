#include "infrastructure/Logging.h"

#include <QDateTime>
#include <QFile>
#include <QMessageLogContext>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>

#include <cstdlib>

namespace
{
QFile g_logFile;
QMutex g_logMutex;
QtMessageHandler g_previousHandler = nullptr;

const char* messageTypeName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return "DEBUG";
    case QtInfoMsg:
        return "INFO";
    case QtWarningMsg:
        return "WARNING";
    case QtCriticalMsg:
        return "CRITICAL";
    case QtFatalMsg:
        return "FATAL";
    }

    return "UNKNOWN";
}

void messageHandler(
    QtMsgType type,
    const QMessageLogContext& context,
    const QString& message)
{
    {
        QMutexLocker locker(&g_logMutex);

        if (g_logFile.isOpen()) {
            QTextStream stream(&g_logFile);
            stream
                << QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
                << " [" << messageTypeName(type) << "]";

            if (context.category && context.category[0] != '\0')
                stream << " [" << context.category << "]";

            stream << ' ' << message << '\n';
            stream.flush();
            g_logFile.flush();
        }
    }

    if (g_previousHandler)
        g_previousHandler(type, context, message);
    else if (type == QtFatalMsg)
        std::abort();
}
}

bool Logging::initialize(const QString& logFilePath)
{
    QMutexLocker locker(&g_logMutex);

    if (g_logFile.isOpen())
        return true;

    g_logFile.setFileName(logFilePath);

    if (!g_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return false;

    g_previousHandler = qInstallMessageHandler(messageHandler);
    return true;
}

void Logging::shutdown()
{
    qInstallMessageHandler(g_previousHandler);
    g_previousHandler = nullptr;

    QMutexLocker locker(&g_logMutex);
    g_logFile.close();
}
