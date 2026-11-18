#include "systemdbackend.h"

#include <QByteArray>
#include <QTimer>

#include <memory>

using namespace Zapret::Platform;

namespace {
constexpr qsizetype QueryOutputLimit = 4096;

ZapretBackend::State parseUnitState(const QString &output)
{
    if (output == QLatin1String("active"))
        return ZapretBackend::State::Active;
    if (output == QLatin1String("inactive"))
        return ZapretBackend::State::Inactive;
    if (output == QLatin1String("failed"))
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

} // namespace

SystemdBackend::SystemdBackend(ServiceControlOperations *operations, QObject *parent)
    : ZapretBackend(parent)
    , m_operations(operations)
{
    Q_ASSERT(m_operations);
    m_operations->setParent(this);
}

int SystemdBackend::processTimeoutMs(bool privileged) const
{
    return privileged ? 120000 : 15000;
}

QString SystemdBackend::actionName(UnitAction action)
{
    switch (action) {
    case UnitAction::Start:
        return QStringLiteral("start");
    case UnitAction::Stop:
        return QStringLiteral("stop");
    case UnitAction::Enable:
        return QStringLiteral("enable");
    case UnitAction::Disable:
        return QStringLiteral("disable");
    }

    return QStringLiteral("unknown");
}

Zapret::Backend::Operation SystemdBackend::operationForAction(UnitAction action)
{
    using Zapret::Backend::Operation;

    switch (action) {
    case UnitAction::Start:
        return Operation::Start;
    case UnitAction::Stop:
        return Operation::Stop;
    case UnitAction::Enable:
    case UnitAction::Disable:
        return Operation::SetBootEnabled;
    }

    return Operation::QueryState;
}

QString SystemdBackend::queryErrorMessage(const OperationResult &result) const
{
    switch (result.status) {
    case OperationStatus::TimedOut:
        return tr("Превышено время ожидания ответа systemctl.");
    case OperationStatus::ProtocolError:
        return tr("Некорректный ответ systemctl.");
    case OperationStatus::Unavailable:
        return tr("Не удалось запустить systemctl.");
    case OperationStatus::Failed:
    case OperationStatus::AuthorizationDenied:
        return tr("Ошибка systemctl: %1").arg(result.nativeCode);
    case OperationStatus::Succeeded:
        break;
    }

    return {};
}

QString SystemdBackend::actionErrorMessage(
    OperationStatus status, int nativeCode, const QString &action) const
{
    switch (status) {
    case OperationStatus::AuthorizationDenied:
        return tr("Авторизация для операции systemd отклонена.");
    case OperationStatus::TimedOut:
        return tr("Истекло время ожидания операции systemd: %1").arg(action);
    case OperationStatus::Unavailable:
        return tr("Не удалось запустить операцию systemd.");
    case OperationStatus::ProtocolError:
        return tr("Некорректный ответ systemd.");
    case OperationStatus::Failed:
        return tr("Операция не выполнена (код %1).").arg(nativeCode);
    case OperationStatus::Succeeded:
        break;
    }

    return {};
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

void SystemdBackend::observeQuery(PlatformOperation *operation, const QueryCallback &callback)
{
    if (!operation) {
        QTimer::singleShot(0, this, [callback] {
            callback({}, {OperationStatus::Unavailable, 0,
                          QStringLiteral("systemctl.operation_unavailable")});
        });
        return;
    }

    auto output = std::make_shared<QByteArray>();
    auto outputExceeded = std::make_shared<bool>(false);
    auto completed = std::make_shared<bool>(false);
    auto *timeout = new QTimer(this);
    timeout->setSingleShot(true);

    connect(timeout, &QTimer::timeout, this,
            [this, timeout, completed, callback] {
        if (*completed)
            return;
        *completed = true;
        timeout->deleteLater();
        const OperationResult result{
            OperationStatus::TimedOut, 0, QStringLiteral("systemctl.timeout")
        };
        emit errorOccurred(queryErrorMessage(result));
        callback({}, result);
    });

    connect(operation, &PlatformOperation::standardOutputReceived, this,
            [output, outputExceeded, completed](const QByteArray &data) {
        if (*completed || data.isEmpty())
            return;
        if (output->size() + data.size() > QueryOutputLimit) {
            *outputExceeded = true;
            output->clear();
            return;
        }
        output->append(data);
    });

    connect(operation, &PlatformOperation::finished, this,
            [this, timeout, output, outputExceeded, completed, callback]
            (const OperationResult &result) {
        if (*completed)
            return;
        *completed = true;
        timeout->stop();
        timeout->deleteLater();

        OperationResult finalResult = result;
        QString decoded;
        if (*outputExceeded) {
            finalResult = {OperationStatus::ProtocolError, result.nativeCode,
                           QStringLiteral("systemctl.stdout_limit_exceeded")};
        } else if (result.succeeded()) {
            decoded = QString::fromUtf8(*output).trimmed();
            if (decoded.toUtf8() != output->trimmed()) {
                finalResult = {OperationStatus::ProtocolError, result.nativeCode,
                               QStringLiteral("systemctl.invalid_utf8")};
                decoded.clear();
            } else if (decoded.isEmpty()) {
                finalResult = {OperationStatus::Failed, result.nativeCode,
                               QStringLiteral("systemctl.empty_response")};
            }
        }

        if (!finalResult.succeeded())
            emit errorOccurred(queryErrorMessage(finalResult));
        callback(decoded, finalResult);
    });

    timeout->start(processTimeoutMs(false));
}

void SystemdBackend::queryEngineState(const QueryCallback &callback)
{
    observeQuery(m_operations->queryEngineState(), callback);
}

void SystemdBackend::queryBootEnabled(const QueryCallback &callback)
{
    observeQuery(m_operations->queryBootEnabled(), callback);
}

void SystemdBackend::refresh()
{
    if (m_refreshInProgress)
        return;

    m_refreshInProgress = true;
    m_refreshQueriesPending = 2;
    const quint64 generation = ++m_stateQueryGeneration;

    queryEngineState([this, generation](const QString &output, const OperationResult &result) {
        if (generation == m_stateQueryGeneration) {
            const State state = result.succeeded() ? parseUnitState(output) : State::Unknown;
            emit stateChanged(state);
        }
        finishRefreshQuery();
    });

    queryBootEnabled([this, generation](const QString &output, const OperationResult &result) {
        if (generation == m_stateQueryGeneration)
            emit bootEnabledChanged(result.succeeded() && output == QLatin1String("enabled"));
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

    queryEngineState([this, verificationGeneration, stateGeneration](
                         const QString &output, const OperationResult &result) {
        if (verificationGeneration != m_verificationGeneration)
            return;

        const State state = result.succeeded() ? parseUnitState(output) : State::Unknown;
        if (stateGeneration == m_stateQueryGeneration)
            emit stateChanged(state);
        emit stateVerified(state);
    });
}

void SystemdBackend::verifyState(quint64 requestId)
{
    ++m_stateQueryGeneration;
    const quint64 verificationGeneration = ++m_verificationGeneration;

    queryEngineState([this, requestId, verificationGeneration](
                         const QString &output, const OperationResult &result) {
        if (verificationGeneration == m_verificationGeneration) {
            const State state = result.succeeded() ? parseUnitState(output) : State::Unknown;
            emit stateVerifiedForRequest(requestId, state);
        }
    });
}

void SystemdBackend::start() { runUnitAction(UnitAction::Start); }
void SystemdBackend::start(quint64 requestId) { runUnitAction(UnitAction::Start, requestId); }
void SystemdBackend::stop() { runUnitAction(UnitAction::Stop); }
void SystemdBackend::stop(quint64 requestId) { runUnitAction(UnitAction::Stop, requestId); }

void SystemdBackend::setBootEnabled(bool enabled)
{
    runUnitAction(enabled ? UnitAction::Enable : UnitAction::Disable);
}

void SystemdBackend::setBootEnabled(bool enabled, quint64 requestId)
{
    runUnitAction(enabled ? UnitAction::Enable : UnitAction::Disable, requestId);
}

void SystemdBackend::runUnitAction(UnitAction action, quint64 requestId)
{
    m_pendingActions.enqueue({action, requestId});
    startNextUnitAction();
}

void SystemdBackend::startNextUnitAction()
{
    if (m_actionInProgress || m_pendingActions.isEmpty())
        return;

    m_actionInProgress = true;
    ++m_stateQueryGeneration;
    ++m_verificationGeneration;

    const PendingUnitAction pending = m_pendingActions.dequeue();
    PlatformOperation *operation = nullptr;
    switch (pending.action) {
    case UnitAction::Start:
        operation = m_operations->startEngine();
        break;
    case UnitAction::Stop:
        operation = m_operations->stopEngine();
        break;
    case UnitAction::Enable:
        operation = m_operations->setBootEnabled(true);
        break;
    case UnitAction::Disable:
        operation = m_operations->setBootEnabled(false);
        break;
    }

    const QString action = actionName(pending.action);
    auto timedOut = std::make_shared<bool>(false);
    auto completed = std::make_shared<bool>(false);
    auto *warningTimer = new QTimer(this);
    warningTimer->setSingleShot(true);
    connect(warningTimer, &QTimer::timeout, this,
            [this, action, timedOut] {
        *timedOut = true;
        emit errorOccurred(tr("Операция systemd выполняется дольше ожидаемого: %1. Ожидается фактическое завершение команды.")
                               .arg(action));
    });
    warningTimer->start(processTimeoutMs(true));

    if (!operation) {
        *completed = true;
        warningTimer->stop();
        warningTimer->deleteLater();
        const QString message = actionErrorMessage(
            OperationStatus::Unavailable, 0, action);
        emit errorOccurred(message);
        completeUnitAction(pending.action, pending.requestId, false,
                           Zapret::Backend::OperationStatus::Failed, message);
        return;
    }

    connect(operation, &PlatformOperation::finished, this,
            [this, pending, action, timedOut, completed, warningTimer]
            (const OperationResult &result) {
        if (*completed)
            return;
        *completed = true;
        warningTimer->stop();
        warningTimer->deleteLater();

        OperationStatus status = result.status;
        if (*timedOut)
            status = OperationStatus::TimedOut;

        const bool success = status == OperationStatus::Succeeded;
        const QString message = actionErrorMessage(status, result.nativeCode, action);
        if (!success)
            emit errorOccurred(message);

        const auto backendStatus = success
            ? Zapret::Backend::OperationStatus::Succeeded
            : (status == OperationStatus::AuthorizationDenied
                   ? Zapret::Backend::OperationStatus::AuthorizationDenied
                   : (status == OperationStatus::TimedOut
                          ? Zapret::Backend::OperationStatus::TimedOut
                          : Zapret::Backend::OperationStatus::Failed));
        completeUnitAction(pending.action, pending.requestId, success,
                           backendStatus, message);
    });
}

void SystemdBackend::completeUnitAction(
    UnitAction action, quint64 requestId, bool success,
    Zapret::Backend::OperationStatus status, const QString &message)
{
    ++m_verificationGeneration;
    const quint64 generation = ++m_stateQueryGeneration;

    queryEngineState([this, action, requestId, success, status, message, generation](
                         const QString &output, const OperationResult &queryResult) {
        const State state = queryResult.succeeded() ? parseUnitState(output) : State::Unknown;
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
            emit unitActionFinished(actionName(action), state, success);
        else
            emit unitActionFinishedForRequest(requestId, actionName(action), state, success);

        m_actionInProgress = false;
        startNextUnitAction();
    });

    queryBootEnabled([this, generation](const QString &output, const OperationResult &result) {
        if (generation == m_stateQueryGeneration)
            emit bootEnabledChanged(result.succeeded() && output == QLatin1String("enabled"));
    });
}
