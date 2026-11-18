#pragma once

#include <QObject>
#include <QString>

// Platform-neutral contract for desktop tray integration.
// Concrete implementations belong to the platform integration layer.
class TrayIntegration : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool watcherAvailable READ watcherAvailable NOTIFY watcherAvailableChanged)
    Q_PROPERTY(bool registered READ registered NOTIFY registeredChanged)
    Q_PROPERTY(bool registrationFailed READ registrationFailed NOTIFY registrationFailedChanged)

public:
    explicit TrayIntegration(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    ~TrayIntegration() override = default;

    virtual void setServiceState(const QString &state, const QString &stateCode,
                                 bool active, bool busy) = 0;
    virtual void showNotification(const QString &title, const QString &message) = 0;

    virtual bool watcherAvailable() const = 0;
    virtual bool registered() const = 0;
    virtual bool registrationFailed() const = 0;

signals:
    void watcherAvailableChanged();
    void registeredChanged();
    void registrationFailedChanged();
    void activateRequested();
    void settingsRequested();
    void quitRequested();
    void startRequested();
    void stopRequested();
};
