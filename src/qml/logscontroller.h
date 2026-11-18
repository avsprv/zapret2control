#pragma once

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QString>

#include "platform/privilegedoperations.h"

class LogsController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString logText READ logText NOTIFY logTextChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(int lineCount READ lineCount NOTIFY lineCountChanged)

public:
    explicit LogsController(Zapret::Platform::SystemLogOperations *operations,
                            QObject *parent = nullptr);

    QString logText() const;
    QString status() const;
    bool busy() const;
    int lineCount() const;

    Q_INVOKABLE void readLogs(int lineCount = 200);

signals:
    void logTextChanged();
    void statusChanged();
    void busyChanged();
    void lineCountChanged();

private:
    void setStatus(const QString &status);
    void setBusy(bool busy);
    void setLogText(const QString &text);
    QString operationErrorText(Zapret::Platform::OperationStatus status,
                                int nativeCode) const;

    Zapret::Platform::SystemLogOperations *m_operations = nullptr;
    QPointer<Zapret::Platform::PlatformOperation> m_operation;
    QString m_logText;
    QString m_status;
    int m_lineCount = 200;
    bool m_busy = false;
    QByteArray m_output;
    bool m_outputExceeded = false;
};
