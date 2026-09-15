#pragma once

#include <QString>

namespace Logging
{
[[nodiscard]] bool initialize(const QString& logFilePath);
void shutdown();
}
