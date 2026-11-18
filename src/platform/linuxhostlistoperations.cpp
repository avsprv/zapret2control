#include "linuxhostlistoperations.h"

#include "backend/helperpath.h"

#include <QProcess>
#include <QTimer>

#include <memory>
#include <optional>

namespace Zapret::Platform {
namespace {

constexpr qsizetype HostListLimit = 1024 * 1024;
constexpr qsizetype DiagnosticLimit = 64 * 1024;
constexpr int MinimumTimeoutMs = 100;

std::optional<QString> helperNameForListId(HostListId listId)
{
    switch (listId) {
    case HostListId::ZapretHostsUser:
        return QStringLiteral("zapret-hosts-user.txt");
    case HostListId::ZapretHostsUserExclude:
        return QStringLiteral("zapret-hosts-user-exclude.txt");
    case HostListId::ZapretHostsUserIpBan:
        return QStringLiteral("zapret-hosts-user-ipban.txt");
    case HostListId::ZapretIpUser:
        return QStringLiteral("zapret-ip-user.txt");
    case HostListId::ZapretIpUser6:
        return QStringLiteral("zapret-ip-user6.txt");
    case HostListId::ZapretIpExclude:
        return QStringLiteral("zapret-ip-exclude.txt");
    case HostListId::ZapretIpExclude6:
        return QStringLiteral("zapret-ip-exclude6.txt");
    case HostListId::ZapretIpUserIpBan:
        return QStringLiteral("zapret-ip-user-ipban.txt");
    case HostListId::ZapretIpUserIpBan6:
        return QStringLiteral("zapret-ip-user-ipban6.txt");
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

LinuxHostListOperations::LinuxHostListOperations(int timeoutMs, QObject *parent)
    : QObject(parent)
    , m_timeoutMs(qMax(timeoutMs, MinimumTimeoutMs))
{
}

PlatformOperation *LinuxHostListOperations::readList(HostListId listId)
{
    const auto name = helperNameForListId(listId);
    if (!name.has_value()) {
        return completedOperation(this, {
            OperationStatus::ProtocolError, 0, QStringLiteral("hostlist.invalid_id")
        });
    }

    return startHelperOperation(
        {QStringLiteral("lists-read"), *name}, {}, HostListLimit, DiagnosticLimit, true);
}

PlatformOperation *LinuxHostListOperations::writeList(
    HostListId listId, const QByteArray &contents)
{
    const auto name = helperNameForListId(listId);
    if (!name.has_value()) {
        return completedOperation(this, {
            OperationStatus::ProtocolError, 0, QStringLiteral("hostlist.invalid_id")
        });
    }

    if (listId != HostListId::ZapretHostsUser
        && listId != HostListId::ZapretHostsUserExclude
        && listId != HostListId::ZapretHostsUserIpBan) {
        return completedOperation(this, {
            OperationStatus::ProtocolError, 0, QStringLiteral("hostlist.read_only")
        });
    }

    if (contents.size() > HostListLimit) {
        return completedOperation(this, {
            OperationStatus::ProtocolError, 0, QStringLiteral("hostlist.input_limit_exceeded")
        });
    }

    return startHelperOperation(
        {QStringLiteral("lists-write"), *name}, contents, DiagnosticLimit, DiagnosticLimit, false);
}

PlatformOperation *LinuxHostListOperations::startHelperOperation(
    const QStringList &arguments, const QByteArray &standardInput,
    qsizetype standardOutputLimit, qsizetype standardErrorLimit,
    bool missingListIsSuccess)
{
    auto *operation = new PlatformOperation(this);
    auto *process = new QProcess(this);
    auto *timeout = new QTimer(process);
    auto timedOut = std::make_shared<bool>(false);
    auto inputFailed = std::make_shared<bool>(false);
    auto outputExceeded = std::make_shared<bool>(false);
    auto errorExceeded = std::make_shared<bool>(false);
    auto outputSize = std::make_shared<qsizetype>(0);
    auto errorOutput = std::make_shared<QByteArray>();

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
            [process, operation, standardOutputLimit, outputSize, outputExceeded, missingListIsSuccess] {
        const QByteArray data = process->readAllStandardOutput();
        if (data.isEmpty() || operation->isFinished())
            return;

        *outputSize += data.size();
        if (*outputSize > standardOutputLimit) {
            *outputExceeded = true;
            process->kill();
            return;
        }

        if (missingListIsSuccess)
            operation->reportStandardOutput(data);
    });

    connect(process, &QProcess::readyReadStandardError, operation,
            [process, operation, standardErrorLimit, errorOutput, errorExceeded] {
        const QByteArray data = process->readAllStandardError();
        if (data.isEmpty() || operation->isFinished())
            return;

        if (errorOutput->size() + data.size() > standardErrorLimit) {
            *errorExceeded = true;
            process->kill();
            return;
        }

        // Helper diagnostics are intentionally kept inside the adapter. They are
        // used only to recognize the documented missing-list result and are never
        // forwarded to the application UI.
        errorOutput->append(data);
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
            [process, operation, timeout, timedOut, inputFailed, outputExceeded,
             errorExceeded, outputSize, errorOutput, standardOutputLimit,
             standardErrorLimit, missingListIsSuccess]
            (int exitCode, QProcess::ExitStatus exitStatus) {
        timeout->stop();

        const QByteArray remainingOutput = process->readAllStandardOutput();
        if (!remainingOutput.isEmpty() && !operation->isFinished()) {
            *outputSize += remainingOutput.size();
            if (*outputSize > standardOutputLimit)
                *outputExceeded = true;
            else if (missingListIsSuccess)
                operation->reportStandardOutput(remainingOutput);
        }

        const QByteArray remainingError = process->readAllStandardError();
        if (!remainingError.isEmpty() && !operation->isFinished()) {
            if (errorOutput->size() + remainingError.size() > standardErrorLimit)
                *errorExceeded = true;
            else
                errorOutput->append(remainingError);
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
        } else if (missingListIsSuccess && exitCode == 3
                   && errorOutput->contains("list file does not exist")) {
            result = {OperationStatus::Succeeded, 0,
                      QStringLiteral("hostlist.not_found")};
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
