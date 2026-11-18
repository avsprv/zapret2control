#pragma once

#include "platform/privilegedoperations.h"

#include <QObject>
#include <QStringList>

namespace Zapret::Platform {

class LinuxConfigurationOperations final : public QObject, public ConfigurationOperations
{
    Q_OBJECT

public:
    explicit LinuxConfigurationOperations(int timeoutMs = 30000, QObject *parent = nullptr);

    PlatformOperation *readConfiguration() override;
    PlatformOperation *writeConfiguration(
        const QByteArray &contents, const Sha256Digest &expectedDigest) override;
    PlatformOperation *rollbackConfiguration() override;

private:
    PlatformOperation *startHelperOperation(
        const QStringList &arguments, const QByteArray &standardInput,
        qsizetype standardOutputLimit, qsizetype standardErrorLimit);

    int m_timeoutMs;
};

} // namespace Zapret::Platform
