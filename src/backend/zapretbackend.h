#pragma once

#include <QObject>
#include <QString>

#include "backendtypes.h"

// Единственная точка контакта с upstream engine lifecycle.
class ZapretBackend : public QObject
{
    Q_OBJECT
public:
    enum class State { Unknown, Active, Inactive, Failed };
    Q_ENUM(State)

    using QObject::QObject;
    ~ZapretBackend() override = default;

    virtual void refresh() = 0;
    virtual void verifyState() = 0;
    virtual void verifyState(quint64 requestId) = 0;
    virtual void start() = 0;
    virtual void start(quint64 requestId) = 0;
    virtual void stop() = 0;
    virtual void stop(quint64 requestId) = 0;
    virtual void setBootEnabled(bool enabled) = 0;
    virtual void setBootEnabled(bool enabled, quint64 requestId) = 0;

signals:
    void stateChanged(ZapretBackend::State state);
    void stateVerified(ZapretBackend::State state);
    void stateVerifiedForRequest(quint64 requestId, ZapretBackend::State state);
    void unitActionFinished(const QString &action, ZapretBackend::State state, bool success);
    void unitActionFinishedForRequest(quint64 requestId, const QString &action,
                                      ZapretBackend::State state, bool success);
    void bootEnabledChanged(bool enabled);
    void errorOccurred(const QString &message);
    void operationFinished(const Zapret::Backend::OperationResult &result);
};
