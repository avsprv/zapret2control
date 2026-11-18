#include "systemdbackend.h"

#include <QProcess>
#include <QTimer>
#include <memory>

#include "helperpath.h"

int SystemdBackend::processTimeoutMs(bool privileged) const
{
    return privileged ? 120000 : 15000;
}

void SystemdBackend::startProcess(QProcess *process, const QString &program,
                                   const QStringList &arguments, bool privileged)
{
    if (privileged)
        Helper::start(process, arguments);
    else
        process->start(program, arguments);
}

SystemdBackend::SystemdBackend(QObject *parent)
    : ZapretBackend(parent)
    , m_unit(QStringLiteral("zapret2.service"))
{
}

QList<Zapret::Backend::CapabilityInfo> SystemdBackend::capabilities() const
{
    using namespace Zapret::Backend;

    return {
        {Capability::QueryState, CapabilityAvailability::Available, false, {}},
        {Capability::Start, CapabilityAvailability::Available, true, {}},
        {Capability::Stop, CapabilityAvailability::Available, true, {}},
        {Capability::BootEnablement, CapabilityAvailability::Available, true, {}},
        {Capability::Configuration, CapabilityAvailability::Unsupported, false,
         tr("Управление конфигурацией выполняется отдельным сервисом платформы.")},
        {Capability::Diagnostics, CapabilityAvailability::Unsupported, false,
         tr("Диагностика предоставляется отдельным сервисом платформы.")}
    };
}

void SystemdBackend::query(const QString &verb, const std::function<void(const QString &)> &onResult)
{
    auto *proc = new QProcess(this);
    auto completed = std::make_shared<bool>(false);
    auto timedOut = std::make_shared<bool>(false);
    auto *timeout = new QTimer(proc);
    timeout->setSingleShot(true);
    connect(timeout, &QTimer::timeout, this, [this, proc, completed, timedOut] {
        if (*completed)
            return;
        *timedOut = true;
        emit errorOccurred(tr("Превышено время ожидания ответа systemctl."));
        proc->kill();
    });
    connect(proc, &QProcess::finished, this, [proc, onResult, completed, timedOut, timeout] {
        if (*completed)
            return;
        *completed = true;
        timeout->stop();
        const QString output = *timedOut
            ? QString() : QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        onResult(output);
        proc->deleteLater();
    });
    connect(proc, &QProcess::errorOccurred, this,
            [this, proc, onResult, completed, timeout](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || *completed)
            return;
        *completed = true;
        timeout->stop();
        emit errorOccurred(tr("Ошибка systemctl: %1").arg(proc->errorString()));
        onResult(QString());
        proc->deleteLater();
    });
    startProcess(proc, QStringLiteral("systemctl"), {verb, m_unit}, false);
    timeout->start(processTimeoutMs(false));
}

namespace {
ZapretBackend::State parseUnitState(const QString &out)
{
    if (out == QLatin1String("active"))
        return ZapretBackend::State::Active;
    if (out == QLatin1String("inactive"))
        return ZapretBackend::State::Inactive;
    if (out == QLatin1String("failed"))
        return ZapretBackend::State::Failed;
    return ZapretBackend::State::Unknown;
}

Zapret::Backend::EngineState toEngineState(ZapretBackend::State state)
{
    using Zapret::Backend::EngineState;

    switch (state) {
    case ZapretBackend::State::Active:
        return EngineState::Active;
    case ZapretBackend::State::Inactive:
        return EngineState::Inactive;
    case ZapretBackend::State::Failed:
        return EngineState::Failed;
    case ZapretBackend::State::Unknown:
    default:
        return EngineState::Unknown;
    }
}

Zapret::Backend::Operation operationForAction(const QString &action)
{
    using Zapret::Backend::Operation;

    if (action == QLatin1String("start"))
        return Operation::Start;
    if (action == QLatin1String("stop"))
        return Operation::Stop;
    return Operation::SetBootEnabled;
}
} // namespace

void SystemdBackend::refresh()
{
    // Coalesce periodic refreshes while either systemctl query is still running.
    if (m_refreshInProgress)
        return;

    m_refreshInProgress = true;
    m_refreshQueriesPending = 2;
    const quint64 generation = ++m_stateQueryGeneration;
    query(QStringLiteral("is-active"), [this, generation](const QString &out) {
        if (generation == m_stateQueryGeneration)
            emit stateChanged(parseUnitState(out));
        finishRefreshQuery();
    });
    query(QStringLiteral("is-enabled"), [this, generation](const QString &out) {
        if (generation == m_stateQueryGeneration)
            emit bootEnabledChanged(out == QLatin1String("enabled"));
        finishRefreshQuery();
    });
}

void SystemdBackend::finishRefreshQuery()
{
    if (!m_refreshInProgress || m_refreshQueriesPending <= 0)
        return;

    --m_refreshQueriesPending;
    if (m_refreshQueriesPending == 0)
        m_refreshInProgress = false;
}

void SystemdBackend::verifyState()
{
    const quint64 verificationGeneration = ++m_verificationGeneration;
    const quint64 stateGeneration = m_stateQueryGeneration;
    query(QStringLiteral("is-active"), [this, verificationGeneration, stateGeneration](const QString &out) {
        if (verificationGeneration != m_verificationGeneration)
            return;
        const State state = parseUnitState(out);
        if (stateGeneration == m_stateQueryGeneration)
            emit stateChanged(state);
        emit stateVerified(state);
    });
}

void SystemdBackend::verifyState(quint64 requestId)
{
    // Invalidate older refresh and lifecycle-verification results.
    ++m_stateQueryGeneration;
    const quint64 verificationGeneration = ++m_verificationGeneration;
    // Lifecycle verification is private to its requesting operation. Publishing
    // it as stateChanged would let a late response overwrite the application UI.
    query(QStringLiteral("is-active"), [this, requestId, verificationGeneration](const QString &out) {
        if (verificationGeneration == m_verificationGeneration)
            emit stateVerifiedForRequest(requestId, parseUnitState(out));
    });
}

void SystemdBackend::start() { runUnitAction(QStringLiteral("start")); }
void SystemdBackend::start(quint64 requestId) { runUnitAction(QStringLiteral("start"), requestId); }
void SystemdBackend::stop() { runUnitAction(QStringLiteral("stop")); }
void SystemdBackend::stop(quint64 requestId) { runUnitAction(QStringLiteral("stop"), requestId); }

void SystemdBackend::setBootEnabled(bool enabled)
{
    runUnitAction(enabled ? QStringLiteral("enable") : QStringLiteral("disable"));
}

void SystemdBackend::setBootEnabled(bool enabled, quint64 requestId)
{
    runUnitAction(enabled ? QStringLiteral("enable") : QStringLiteral("disable"), requestId);
}

void SystemdBackend::runUnitAction(const QString &action, quint64 requestId)
{
    m_pendingActions.enqueue({action, requestId});
    startNextUnitAction();
}

void SystemdBackend::startNextUnitAction()
{
    if (m_actionInProgress || m_pendingActions.isEmpty())
        return;

    m_actionInProgress = true;
    ++m_stateQueryGeneration; // invalidate refreshes started before this action
    ++m_verificationGeneration; // invalidate lifecycle checks started before this action
    const PendingUnitAction pending = m_pendingActions.dequeue();
    const QString action = pending.action;
    const quint64 requestId = pending.requestId;
    auto *proc = new QProcess(this);
    auto completed = std::make_shared<bool>(false);
    auto timedOut = std::make_shared<bool>(false);
    auto *timeout = new QTimer(proc);
    timeout->setSingleShot(true);
    const auto reportResult = [this, action, requestId](
                                  bool success,
                                  Zapret::Backend::OperationStatus status,
                                  const QString &message) {
        // Discard lifecycle verification started while the privileged action was running.
        ++m_verificationGeneration;
        const quint64 generation = ++m_stateQueryGeneration;
        query(QStringLiteral("is-active"),
              [this, action, requestId, success, status, message, generation](const QString &out) {
            const State state = parseUnitState(out);
            if (generation == m_stateQueryGeneration)
                emit stateChanged(state);

            const Zapret::Backend::OperationResult result{
                requestId,
                operationForAction(action),
                status,
                toEngineState(state),
                message
            };
            emit operationFinished(result);

            if (requestId == 0)
                emit unitActionFinished(action, state, success);
            else
                emit unitActionFinishedForRequest(requestId, action, state, success);

            m_actionInProgress = false;
            startNextUnitAction();
        });
        query(QStringLiteral("is-enabled"), [this, generation](const QString &out) {
            if (generation == m_stateQueryGeneration)
                emit bootEnabledChanged(out == QLatin1String("enabled"));
        });
    };

    connect(timeout, &QTimer::timeout, this,
            [this, proc, action, completed, timedOut] {
        if (*completed)
            return;
        *timedOut = true;
        emit errorOccurred(tr("Операция systemd выполняется дольше ожидаемого: %1. Ожидается фактическое завершение команды.").arg(action));
    });
    connect(proc, &QProcess::finished, this,
            [this, proc, action, reportResult, completed, timedOut, timeout](int code, QProcess::ExitStatus status) {
        if (*completed)
            return;
        *completed = true;
        timeout->stop();
        const bool success = !*timedOut && code == 0 && status == QProcess::NormalExit;
        const bool authorizationDenied = !success && !*timedOut && Helper::isAuthorizationDenied(code);
        const auto operationStatus = success
            ? Zapret::Backend::OperationStatus::Succeeded
            : (*timedOut
                   ? Zapret::Backend::OperationStatus::TimedOut
                   : (authorizationDenied
                          ? Zapret::Backend::OperationStatus::AuthorizationDenied
                          : Zapret::Backend::OperationStatus::Failed));
        QString message;
        if (*timedOut) {
            message = tr("Истекло время ожидания операции systemd: %1").arg(action);
        } else if (authorizationDenied) {
            message = tr("Авторизация для операции systemd отклонена.");
        } else if (!success) {
            const QString err = QString::fromUtf8(proc->readAllStandardError()).trimmed();
            message = tr("Операция не выполнена (код %1): %2").arg(code).arg(err);
            emit errorOccurred(message);
        }
        proc->deleteLater();
        reportResult(success, operationStatus, message);
    });
    connect(proc, &QProcess::errorOccurred, this,
            [this, proc, reportResult, completed, timeout](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && !*completed) {
            timeout->stop();
            const QString message =
                tr("Не удалось запустить операцию systemd: %1").arg(proc->errorString());
            emit errorOccurred(message);
            *completed = true;
            proc->deleteLater(); // finished() после FailedToStart не приходит
            reportResult(false, Zapret::Backend::OperationStatus::Failed, message);
        }
    });
    startProcess(proc, QString(), {QStringLiteral("unit"), action}, true);
    timeout->start(processTimeoutMs(true));
}
