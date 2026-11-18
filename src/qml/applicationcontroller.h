#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

#include "backend/zapretbackend.h"

class QTimer;

class ApplicationController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString serviceState READ serviceState NOTIFY serviceStateChanged)
    Q_PROPERTY(bool serviceActive READ serviceActive NOTIFY serviceStateChanged)
    Q_PROPERTY(QString serviceStateCode READ serviceStateCode NOTIFY serviceStateChanged)
    Q_PROPERTY(bool bootEnabled READ bootEnabled NOTIFY bootEnabledChanged)
    Q_PROPERTY(bool operationsBusy READ operationsBusy NOTIFY operationsBusyChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QVariantList capabilities READ capabilities NOTIFY capabilitiesChanged)

public:
    // Takes ownership of the platform backend. Platform selection belongs to the
    // composition layer, not to the QML-facing controller.
    explicit ApplicationController(ZapretBackend *backend, QObject *parent = nullptr);

    QString serviceState() const;
    bool serviceActive() const;
    QString serviceStateCode() const;
    bool bootEnabled() const;
    bool operationsBusy() const;
    void setOperationsBusy(bool busy);
    QString errorMessage() const;
    QVariantList capabilities() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void startService();
    Q_INVOKABLE void stopService();
    Q_INVOKABLE void setBootEnabled(bool enabled);

signals:
    void serviceStateChanged();
    void errorMessageChanged();
    void bootEnabledChanged();
    void operationsBusyChanged();
    void capabilitiesChanged();

private:
    quint64 nextRequestId();
    void setState(ZapretBackend::State state);
    void setServiceActionBusy(bool busy);

    ZapretBackend *m_backend = nullptr;
    QTimer *m_refreshTimer = nullptr;
    ZapretBackend::State m_state = ZapretBackend::State::Unknown;
    quint64 m_nextRequestId = 1;
    quint64 m_pendingRequestId = 0;
    bool m_bootEnabled = false;
    bool m_operationsBusy = false;
    bool m_serviceActionBusy = false;
    QString m_errorMessage;
};
