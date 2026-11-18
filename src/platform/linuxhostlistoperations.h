#pragma once

#include "platform/privilegedoperations.h"

#include <QObject>
#include <QStringList>

namespace Zapret::Platform {

class LinuxHostListOperations final : public QObject, public HostListOperations
{
    Q_OBJECT

public:
    explicit LinuxHostListOperations(int timeoutMs = 30000, QObject *parent = nullptr);

    PlatformOperation *readList(HostListId listId) override;
    PlatformOperation *writeList(HostListId listId, const QByteArray &contents) override;

private:
    PlatformOperation *startHelperOperation(
        const QStringList &arguments, const QByteArray &standardInput,
        qsizetype standardOutputLimit, qsizetype standardErrorLimit,
        bool missingListIsSuccess);

    int m_timeoutMs;
};

} // namespace Zapret::Platform
