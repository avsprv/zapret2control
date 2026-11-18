#include "linuxsystemlogoperations.h"

#include "backend/helperpath.h"

#include <QProcess>
#include <QTimer>

#include <memory>
#include <optional>

namespace Zapret::Platform {
namespace {

constexpr qsizetype LogOutputLimit = 1024 * 1024;
constexpr qsizetype DiagnosticLimit = 64 * 1024;
constexpr int MinimumTimeoutMs = 100;

std::optional<int> lineCountForLimit(LogLineCount lineCount)
{
    switch (lineCount) {
    case LogLineCount::Fifty:
        return 50;
    case LogLineCount::OneHundred:
        return 100;
    case LogLineCount::TwoHundred:
        return 200;
    case LogLineCount::FiveHundred:
        return 500;
    }

    return std::nullopt;
}

OperationResult resultForExitCode(int exitCode)
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

LinuxSystemLogOperations::LinuxSystemLogOperations(int timeoutMs, QObject *parent)
    : QObject(parent)
    , m_timeoutMs(qMax(timeoutMs, MinimumTimeoutMs))
{
}

PlatformOperation *LinuxSystemLogOperations::readRecentLogs(LogLineCount lineCount)
{
    const auto limit = lineCountForLimit(lineCount);
    if (!limit.has_value()) {
        return completedOperation(this, {
            OperationStatus::ProtocolError, 0, QStringLiteral("logs.invalid_line_count")
        });
    }

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
        if (*outputSize > LogOutputLimit) {
            *outputExceeded = true;
            process->kill();
            return;
        }

        operation->reportStandardOutput(data);
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
        // Helper diagnostics are deliberately not forwarded to the UI.
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
        if (!remainingOutput.isEmpty() && !operation->isFinished()) {
            *outputSize += remainingOutput.size();
            if (*outputSize > LogOutputLimit)
                *outputExceeded = true;
            else
                operation->reportStandardOutput(remainingOutput);
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
                      QStringLiteral("helper.timeout")};
        } else if (*outputExceeded || *diagnosticExceeded) {
            result = {OperationStatus::ProtocolError, exitCode,
                      *outputExceeded ? QStringLiteral("helper.stdout_limit_exceeded")
                                      : QStringLiteral("helper.stderr_limit_exceeded")};
        } else if (exitStatus != QProcess::NormalExit) {
            result = {OperationStatus::Failed, exitCode,
                      QStringLiteral("helper.crashed")};
        } else {
            result = resultForExitCode(exitCode);
        }

        operation->finish(result);
        process->deleteLater();
    });

    timeout->start(m_timeoutMs);
    Helper::start(process, {QStringLiteral("logs-read"), QString::number(*limit)});
    return operation;
}

} // namespace Zapret::Platform
