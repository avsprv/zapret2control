#pragma once

#include "platform/trayintegration.h"

#include <QDBusConnection>
#include <QString>

class QDBusServiceWatcher;
class QEvent;
class StatusNotifierItem;
class DBusMenu;

class TrayController final : public TrayIntegration
{
    Q_OBJECT

public:
    explicit TrayController(QObject *parent = nullptr);
    ~TrayController() override;

    void setServiceState(const QString &state, const QString &stateCode,
                         bool active, bool busy) override;
    void showNotification(const QString &title, const QString &message) override;

    bool watcherAvailable() const override;
    bool registered() const override;
    bool registrationFailed() const override;

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void refreshWatcherAvailability();
    void registerWithWatcher();
    void setRegistered(bool registered);
    void setRegistrationFailed(bool failed);

    QDBusConnection m_bus;
    QDBusServiceWatcher *m_watcher = nullptr;
    StatusNotifierItem *m_item = nullptr;
    DBusMenu *m_menu = nullptr;
    QString m_serviceName;
    bool m_watcherAvailable = false;
    bool m_registered = false;
    bool m_registrationFailed = false;
    bool m_exported = false;
    bool m_registrationPending = false;
    QString m_watcherOwner;
};
