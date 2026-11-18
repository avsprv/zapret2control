#pragma once

#include "platform/privilegedoperations.h"

#include <QObject>

namespace Zapret::Platform {

class LinuxSystemLogOperations final : public QObject, public SystemLogOperations
{
    Q_OBJECT

public:
    explicit LinuxSystemLogOperations(int timeoutMs = 30000, QObject *parent = nullptr);

    PlatformOperation *readRecentLogs(LogLineCount lineCount) override;

private:
    int m_timeoutMs;
};

} // namespace Zapret::Platform
