#include "linuxconfigurationoperations.h"

#include "backend/helperpath.h"

#include <QProcess>
#include <QTimer>

#include <memory>

namespace Zapret::Platform {
namespace {

constexpr qsizetype ConfigurationLimit = 1024 * 1024;
constexpr qsizetype DiagnosticLimit = 64 * 1024;
constexpr int MinimumTimeoutMs = 100;

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

} // namespace

LinuxConfigurationOperations::LinuxConfigurationOperations(int timeoutMs, QObject *parent)
    : QObject(parent)
    , m_timeoutMs(qMax(timeoutMs, MinimumTimeoutMs))
{
}

PlatformOperation *LinuxConfigurationOperations::readConfiguration()
{
    return startHelperOperation({QStringLiteral("config-read")}, {},
                                ConfigurationLimit, DiagnosticLimit);
}

PlatformOperation *LinuxConfigurationOperations::writeConfiguration(
    const QByteArray &contents, const Sha256Digest &expectedDigest)
{
    if (contents.size() > ConfigurationLimit) {
        auto *operation = new PlatformOperation(this);
        operation->finish({OperationStatus::ProtocolError, 0,
                           QStringLiteral("configuration.input_limit_exceeded")});
        return operation;
    }

    return startHelperOperation(
        {QStringLiteral("config-write"), QString::fromLatin1(expectedDigest.hex())},
        contents, DiagnosticLimit, DiagnosticLimit);
}

PlatformOperation *LinuxConfigurationOperations::rollbackConfiguration()
{
    return startHelperOperation({QStringLiteral("config-rollback")}, {},
                                DiagnosticLimit, DiagnosticLimit);
}

PlatformOperation *LinuxConfigurationOperations::startHelperOperation(
    const QStringList &arguments, const QByteArray &standardInput,
    qsizetype standardOutputLimit, qsizetype standardErrorLimit)
{
    auto *operation = new PlatformOperation(this);
    auto *process = new QProcess(this);
    auto *timeout = new QTimer(process);
    auto timedOut = std::make_shared<bool>(false);
    auto inputFailed = std::make_shared<bool>(false);
    auto outputExceeded = std::make_shared<bool>(false);
    auto errorExceeded = std::make_shared<bool>(false);
    auto outputSize = std::make_shared<qsizetype>(0);
    auto errorSize = std::make_shared<qsizetype>(0);

    timeout->setSingleShot(true);
    connect(timeout, &QTimer::timeout, process, [process, timedOut] {
        *timedOut = true;
        process->kill();
    });

    connect(process, &QProcess::started, process, [process, standardInput, inputFailed] {
        if (standardInput.isEmpty()) {
            process->closeWriteChannel();
            return;
        }

        if (process->write(standardInput) != standardInput.size()) {
            *inputFailed = true;
            process->kill();
            return;
        }

        process->closeWriteChannel();
    });

    connect(process, &QProcess::readyReadStandardOutput, operation,
            [process, operation, standardOutputLimit, outputSize, outputExceeded] {
        const QByteArray data = process->readAllStandardOutput();
        if (data.isEmpty() || operation->isFinished())
            return;

        *outputSize += data.size();
        if (*outputSize > standardOutputLimit) {
            *outputExceeded = true;
            process->kill();
            return;
        }

        operation->reportStandardOutput(data);
    });

    connect(process, &QProcess::readyReadStandardError, operation,
            [process, operation, standardErrorLimit, errorSize, errorExceeded] {
        const QByteArray data = process->readAllStandardError();
        if (data.isEmpty() || operation->isFinished())
            return;

        *errorSize += data.size();
        if (*errorSize > standardErrorLimit) {
            *errorExceeded = true;
            process->kill();
            return;
        }

        operation->reportStandardError(data);
    });

    connect(process, &QProcess::errorOccurred, operation,
            [process, operation](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || operation->isFinished())
            return;

        operation->reportStandardError(process->errorString().toUtf8());
        operation->finish({OperationStatus::Unavailable, static_cast<int>(error),
                           QStringLiteral("helper.start_failed")});
        process->deleteLater();
    });

    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            operation,
            [process, operation, timeout, timedOut, inputFailed, outputExceeded,
             errorExceeded, outputSize, errorSize, standardOutputLimit, standardErrorLimit]
            (int exitCode, QProcess::ExitStatus exitStatus) {
        timeout->stop();

        const QByteArray remainingOutput = process->readAllStandardOutput();
        if (!remainingOutput.isEmpty() && !operation->isFinished()) {
            *outputSize += remainingOutput.size();
            if (*outputSize > standardOutputLimit)
                *outputExceeded = true;
            else
                operation->reportStandardOutput(remainingOutput);
        }

        const QByteArray remainingError = process->readAllStandardError();
        if (!remainingError.isEmpty() && !operation->isFinished()) {
            *errorSize += remainingError.size();
            if (*errorSize > standardErrorLimit)
                *errorExceeded = true;
            else
                operation->reportStandardError(remainingError);
        }

        if (operation->isFinished()) {
            process->deleteLater();
            return;
        }

        OperationResult result;
        if (*timedOut) {
            result = {OperationStatus::TimedOut, exitCode,
                      QStringLiteral("helper.timeout")};
        } else if (*inputFailed || *outputExceeded || *errorExceeded) {
            const QString code = *inputFailed
                ? QStringLiteral("helper.stdin_failed")
                : (*outputExceeded ? QStringLiteral("helper.stdout_limit_exceeded")
                                   : QStringLiteral("helper.stderr_limit_exceeded"));
            result = {OperationStatus::ProtocolError, exitCode, code};
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
    Helper::start(process, arguments);
    return operation;
}

} // namespace Zapret::Platform
