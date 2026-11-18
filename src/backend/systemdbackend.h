#pragma once

#include <QQueue>
#include <QString>
#include <functional>

#include "zapretbackend.h"
#include "platform/privilegedoperations.h"

class QTimer;

class SystemdBackend : public ZapretBackend
{
    Q_OBJECT

public:
    explicit SystemdBackend(Zapret::Platform::ServiceControlOperations *operations,
                            QObject *parent = nullptr);

    QList<Zapret::Backend::CapabilityInfo> capabilities() const override;

    void refresh() override;
    void verifyState() override;
    void verifyState(quint64 requestId) override;
    void start() override;
    void start(quint64 requestId) override;
    void stop() override;
    void stop(quint64 requestId) override;
    void setBootEnabled(bool enabled) override;
    void setBootEnabled(bool enabled, quint64 requestId) override;

protected:
    // Query timeout is a hard deadline. Action timeout is a soft warning:
    // the FIFO queue remains locked until the adapter reports actual completion.
    virtual int processTimeoutMs(bool privileged) const;

private:
    enum class UnitAction {
        Start,
        Stop,
        Enable,
        Disable
    };

    struct PendingUnitAction {
        UnitAction action = UnitAction::Start;
        quint64 requestId = 0;
    };

    using QueryCallback = std::function<void(
        const QString &, const Zapret::Platform::OperationResult &)>;

    void queryEngineState(const QueryCallback &callback);
    void queryBootEnabled(const QueryCallback &callback);
    void observeQuery(Zapret::Platform::PlatformOperation *operation,
                      const QueryCallback &callback);
    void finishRefreshQuery();
    void runUnitAction(UnitAction action, quint64 requestId = 0);
    void startNextUnitAction();
    void completeUnitAction(UnitAction action, quint64 requestId,
                            bool success, Zapret::Backend::OperationStatus status,
                            const QString &message);

    Zapret::Platform::ServiceControlOperations *m_operations = nullptr;
    QQueue<PendingUnitAction> m_pendingActions;
    bool m_actionInProgress = false;
    bool m_refreshInProgress = false;
    int m_refreshQueriesPending = 0;
    quint64 m_stateQueryGeneration = 0;
    quint64 m_verificationGeneration = 0;
};
