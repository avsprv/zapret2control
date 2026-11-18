#pragma once

#include <QProcess>
#include <QQueue>
#include <QStringList>
#include <functional>

#include "zapretbackend.h"

// Управляет upstream unit zapret2.service.
// Чтение статуса — без привилегий; start/stop/enable/disable — через helper (polkit).
class SystemdBackend : public ZapretBackend
{
    Q_OBJECT
public:
    explicit SystemdBackend(QObject *parent = nullptr);

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
    virtual int processTimeoutMs(bool privileged) const;
    virtual void startProcess(QProcess *process, const QString &program,
                              const QStringList &arguments, bool privileged);

private:
    struct PendingUnitAction {
        QString action;
        quint64 requestId = 0;
    };

    void startNextUnitAction();
    void finishRefreshQuery();
    void runUnitAction(const QString &action, quint64 requestId = 0);
    void query(const QString &verb, const std::function<void(const QString &)> &onResult);

    QString m_unit;
    QQueue<PendingUnitAction> m_pendingActions;
    bool m_actionInProgress = false;
    bool m_refreshInProgress = false;
    int m_refreshQueriesPending = 0;
    quint64 m_stateQueryGeneration = 0;
    quint64 m_verificationGeneration = 0;
};
