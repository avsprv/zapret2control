#pragma once

#include <QObject>
#include <QUrl>
#include <QString>

#include "platform/privilegedoperations.h"

class ConfigController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString configText READ configText WRITE setConfigText NOTIFY configTextChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged)

public:
    explicit ConfigController(Zapret::Platform::ConfigurationOperations *operations,
                              QObject *parent = nullptr);
    QString configText() const;
    void setConfigText(const QString &text);
    QString status() const;
    bool busy() const;
    bool loaded() const;

    Q_INVOKABLE void read();
    Q_INVOKABLE void validate();
    Q_INVOKABLE void save();
    Q_INVOKABLE void rollback();
    Q_INVOKABLE bool exportConfig(const QUrl &url);

signals:
    void configTextChanged();
    void statusChanged();
    void busyChanged();
    void loadedChanged();

private:
    enum class OperationKind {
        Read,
        Save,
        Rollback
    };

    void setStatus(const QString &status);
    void setBusy(bool busy);
    void startOperation(Zapret::Platform::PlatformOperation *operation,
                        OperationKind kind, const QByteArray &input = {});
    void handleOperationFinished(Zapret::Platform::PlatformOperation *operation,
                                 OperationKind kind,
                                 const Zapret::Platform::OperationResult &result);

    Zapret::Platform::ConfigurationOperations *m_operations = nullptr;
    Zapret::Platform::PlatformOperation *m_operation = nullptr;
    QString m_configText;
    QString m_status;
    bool m_busy = false;
    bool m_loaded = false;
    QByteArray m_originalBytes;
    QByteArray m_operationOutput;
    QByteArray m_pendingInput;
};
