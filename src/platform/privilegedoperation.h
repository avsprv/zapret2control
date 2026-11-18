#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

namespace Zapret::Platform {

enum class OperationStatus {
    Succeeded,
    Failed,
    AuthorizationDenied,
    TimedOut,
    Unavailable,
    ProtocolError
};

struct OperationResult {
    OperationStatus status = OperationStatus::Failed;
    int nativeCode = 0;
    QString diagnosticCode;

    bool succeeded() const
    {
        return status == OperationStatus::Succeeded;
    }
};

class PlatformOperation : public QObject
{
    Q_OBJECT

public:
    explicit PlatformOperation(QObject *parent = nullptr);
    ~PlatformOperation() override = default;

    bool isFinished() const;

    // Adapters must enforce operation-specific output limits before forwarding.
    void reportStandardOutput(const QByteArray &data);
    void reportStandardError(const QByteArray &data);
    void finish(const OperationResult &result);

signals:
    void standardOutputReceived(const QByteArray &data);
    void standardErrorReceived(const QByteArray &data);
    void finished(const Zapret::Platform::OperationResult &result);

private:
    bool m_finished = false;
};

} // namespace Zapret::Platform

Q_DECLARE_METATYPE(Zapret::Platform::OperationStatus)
Q_DECLARE_METATYPE(Zapret::Platform::OperationResult)
