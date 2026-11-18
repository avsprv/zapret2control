#pragma once

#include "platform/privilegedoperations.h"

#include <QObject>

namespace Zapret::Platform {

class LinuxServiceControlOperations final : public QObject, public ServiceControlOperations
{
    Q_OBJECT

public:
    explicit LinuxServiceControlOperations(int queryTimeoutMs = 15000,
                                            int actionTimeoutMs = 600000,
                                            QObject *parent = nullptr);

    PlatformOperation *queryEngineState() override;
    PlatformOperation *queryBootEnabled() override;
    PlatformOperation *startEngine() override;
    PlatformOperation *stopEngine() override;
    PlatformOperation *setBootEnabled(bool enabled) override;

private:
    PlatformOperation *startQuery(const QString &verb);
    PlatformOperation *startAction(const QString &action);

    int m_queryTimeoutMs;
    int m_actionTimeoutMs;
};

} // namespace Zapret::Platform
