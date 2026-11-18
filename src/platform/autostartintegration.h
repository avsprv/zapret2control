#pragma once

#include <QObject>
#include <QString>

// Platform-neutral contract for enabling or disabling application login startup.
// Concrete implementations own all operating-system-specific persistence.
class AutostartIntegration : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled NOTIFY enabledChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)

public:
    explicit AutostartIntegration(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    ~AutostartIntegration() override = default;

    virtual bool enabled() const = 0;
    virtual QString errorMessage() const = 0;

    Q_INVOKABLE virtual void setEnabled(bool enabled) = 0;

signals:
    void enabledChanged();
    void errorMessageChanged();
};
