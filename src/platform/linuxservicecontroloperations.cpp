#include "linuxservicecontroloperations.h"

#include "backend/helperpath.h"

#include <QProcess>
#include <QTimer>

#include <memory>

namespace Zapret::Platform {
namespace {

constexpr qsizetype QueryOutputLimit = 4096;
constexpr qsizetype DiagnosticLimit = 64 * 1024;
constexpr int MinimumTimeoutMs = 100;

OperationResult resultForActionExitCode(int exitCode)
{
    if (Helper::isAuthorizationDenied(exitCode)) {
        return {OperationStatus::AuthorizationDenied, exitCode,
                QStringLiteral("helper.authorization_denied")};
    }

    if (exitCode == 0) {
        return {OperationStatus::Succeeded, 0, QStringLiteral("helper.completed")};
    }

    return {OperationStatus::Failed, exitCode, QStringLiteral("helper.exit_failed")};
}

PlatformOperation *completedOperation(QObject *parent, const OperationResult &result)
{
    auto *operation = new PlatformOperation(parent);
    QTimer::singleShot(0, operation, [operation, result] {
        operation->finish(result);
    });
    return operation;
}

} // namespace

LinuxServiceControlOperations::LinuxServiceControlOperations(
    int queryTimeoutMs, int actionTimeoutMs, QObject *parent)
    : QObject(parent)
    , m_queryTimeoutMs(qMax(queryTimeoutMs, MinimumTimeoutMs))
    , m_actionTimeoutMs(qMax(actionTimeoutMs, MinimumTimeoutMs))
{
}

PlatformOperation *LinuxServiceControlOperations::queryEngineState()
{
    return startQuery(QStringLiteral("is-active"));
}

PlatformOperation *LinuxServiceControlOperations::queryBootEnabled()
{
    return startQuery(QStringLiteral("is-enabled"));
}

PlatformOperation *LinuxServiceControlOperations::startEngine()
{
    return startAction(QStringLiteral("start"));
}

PlatformOperation *LinuxServiceControlOperations::stopEngine()
{
    return startAction(QStringLiteral("stop"));
}

PlatformOperation *LinuxServiceControlOperations::setBootEnabled(bool enabled)
{
    return startAction(enabled ? QStringLiteral("enable") : QStringLiteral("disable"));
}

PlatformOperation *LinuxServiceControlOperations::startQuery(const QString &verb)
{
    auto *operation = new PlatformOperation(this);
    auto *process = new QProcess(this);
    auto *timeout = new QTimer(process);
    auto timedOut = std::make_shared<bool>(false);
    auto outputExceeded = std::make_shared<bool>(false);
    auto diagnosticExceeded = std::make_shared<bool>(false);
    auto outputSize = std::make_shared<qsizetype>(0);
    auto diagnosticSize = std::make_shared<qsizetype>(0);

    timeout->setSingleShot(true);
    connect(process, &QProcess::started, process, [process] {
        process->closeWriteChannel();
    });
    connect(timeout, &QTimer::timeout, process, [process, timedOut] {
        *timedOut = true;
        process->kill();
    });

    connect(process, &QProcess::readyReadStandardOutput, operation,
            [process, operation, outputSize, outputExceeded] {
        const QByteArray data = process->readAllStandardOutput();
        if (data.isEmpty() || operation->isFinished())
            return;

        *outputSize += data.size();
        if (*outputSize > QueryOutputLimit) {
            *outputExceeded = true;
            process->kill();
            return;
        }

        operation->reportStandardOutput(data);
    });

    connect(process, &QProcess::readyReadStandardError, operation,
            [process, diagnosticSize, diagnosticExceeded] {
        const QByteArray data = process->readAllStandardError();
        if (data.isEmpty())
            return;

        *diagnosticSize += data.size();
        if (*diagnosticSize > DiagnosticLimit) {
            *diagnosticExceeded = true;
            process->kill();
        }
    });

    connect(process, &QProcess::errorOccurred, operation,
            [process, operation, timeout](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || operation->isFinished())
            return;

        timeout->stop();
        operation->finish({OperationStatus::Unavailable, static_cast<int>(error),
                           QStringLiteral("systemctl.start_failed")});
        process->deleteLater();
    });

    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            operation,
            [process, operation, timeout, timedOut, outputExceeded, diagnosticExceeded,
             outputSize, diagnosticSize, output]
            (int exitCode, QProcess::ExitStatus exitStatus) {
        timeout->stop();

        const QByteArray remainingOutput = process->readAllStandardOutput();
        if (!remainingOutput.isEmpty() && !operation->isFinished()) {
            *outputSize += remainingOutput.size();
            if (*outputSize > QueryOutputLimit)
                *outputExceeded = true;
            else {
                operation->reportStandardOutput(remainingOutput);
            }
        }

        const QByteArray remainingError = process->readAllStandardError();
        if (!remainingError.isEmpty() && !operation->isFinished()) {
            *diagnosticSize += remainingError.size();
            if (*diagnosticSize > DiagnosticLimit)
                *diagnosticExceeded = true;
        }

        if (operation->isFinished()) {
            process->deleteLater();
            return;
        }

        OperationResult result;
        if (*timedOut) {
            result = {OperationStatus::TimedOut, exitCode,
                      QStringLiteral("systemctl.timeout")};
        } else if (*outputExceeded || *diagnosticExceeded) {
            result = {OperationStatus::ProtocolError, exitCode,
                      *outputExceeded ? QStringLiteral("systemctl.stdout_limit_exceeded")
                                      : QStringLiteral("systemctl.stderr_limit_exceeded")};
        } else if (exitStatus != QProcess::NormalExit) {
            result = {OperationStatus::Failed, exitCode,
                      QStringLiteral("systemctl.crashed")};
        } else if (*outputSize == 0) {
            result = {OperationStatus::Failed, exitCode,
                      QStringLiteral("systemctl.empty_response")};
        } else {
            // systemctl is-active/is-enabled use nonzero exit codes for valid states
            // such as inactive or disabled. The bounded stdout is authoritative.
            result = {OperationStatus::Succeeded, exitCode,
                      QStringLiteral("systemctl.query_completed")};
        }

        operation->finish(result);
        process->deleteLater();
    });

    timeout->start(m_queryTimeoutMs);
    process->start(QStringLiteral("systemctl"), {verb, QStringLiteral("zapret2.service")});
    return operation;
}

PlatformOperation *LinuxServiceControlOperations::startAction(const QString &action)
{
    auto *operation = new PlatformOperation(this);
    auto *process = new QProcess(this);
    auto *timeout = new QTimer(process);
    auto timedOut = std::make_shared<bool>(false);
    auto outputExceeded = std::make_shared<bool>(false);
    auto diagnosticExceeded = std::make_shared<bool>(false);
    auto outputSize = std::make_shared<qsizetype>(0);
    auto diagnosticSize = std::make_shared<qsizetype>(0);

    timeout->setSingleShot(true);
    connect(process, &QProcess::started, process, [process] {
        process->closeWriteChannel();
    });
    connect(timeout, &QTimer::timeout, process, [process, timedOut] {
        *timedOut = true;
        process->kill();
    });

    connect(process, &QProcess::readyReadStandardOutput, operation,
            [process, operation, outputSize, outputExceeded] {
        const QByteArray data = process->readAllStandardOutput();
        if (data.isEmpty() || operation->isFinished())
            return;

        *outputSize += data.size();
        if (*outputSize > DiagnosticLimit) {
            *outputExceeded = true;
            process->kill();
        }
        // Service-control output is not user-facing.
    });

    connect(process, &QProcess::readyReadStandardError, operation,
            [process, operation, diagnosticSize, diagnosticExceeded] {
        const QByteArray data = process->readAllStandardError();
        if (data.isEmpty() || operation->isFinished())
            return;

        *diagnosticSize += data.size();
        if (*diagnosticSize > DiagnosticLimit) {
            *diagnosticExceeded = true;
            process->kill();
        }
        // Helper diagnostics are intentionally not forwarded to the UI.
    });

    connect(process, &QProcess::errorOccurred, operation,
            [process, operation, timeout](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || operation->isFinished())
            return;

        timeout->stop();
        operation->finish({OperationStatus::Unavailable, static_cast<int>(error),
                           QStringLiteral("helper.start_failed")});
        process->deleteLater();
    });

    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            operation,
            [process, operation, timeout, timedOut, outputExceeded, diagnosticExceeded,
             outputSize, diagnosticSize](int exitCode, QProcess::ExitStatus exitStatus) {
        timeout->stop();

        const QByteArray remainingOutput = process->readAllStandardOutput();
        if (!remainingOutput.isEmpty()) {
            *outputSize += remainingOutput.size();
            if (*outputSize > DiagnosticLimit)
                *outputExceeded = true;
        }

        const QByteArray remainingError = process->readAllStandardError();
        if (!remainingError.isEmpty()) {
            *diagnosticSize += remainingError.size();
            if (*diagnosticSize > DiagnosticLimit)
                *diagnosticExceeded = true;
        }

        if (operation->isFinished()) {
            process->deleteLater();
            return;
        }

        OperationResult result;
        if (*timedOut) {
            result = {OperationStatus::TimedOut, exitCode,
                      QStringLiteral("helper.timeout")};
        } else if (*outputExceeded || *diagnosticExceeded) {
            result = {OperationStatus::ProtocolError, exitCode,
                      *outputExceeded ? QStringLiteral("helper.stdout_limit_exceeded")
                                      : QStringLiteral("helper.stderr_limit_exceeded")};
        } else if (exitStatus != QProcess::NormalExit) {
            result = {OperationStatus::Failed, exitCode,
                      QStringLiteral("helper.crashed")};
        } else {
            result = resultForActionExitCode(exitCode);
        }

        operation->finish(result);
        process->deleteLater();
    });

    timeout->start(m_actionTimeoutMs);
    Helper::start(process, {QStringLiteral("unit"), action});
    return operation;
}

} // namespace Zapret::Platform
