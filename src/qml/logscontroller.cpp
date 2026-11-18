#include "logscontroller.h"

#include <optional>

using namespace Zapret::Platform;

namespace {

std::optional<LogLineCount> logLineCountForValue(int value)
{
    switch (value) {
    case 50:
        return LogLineCount::Fifty;
    case 100:
        return LogLineCount::OneHundred;
    case 200:
        return LogLineCount::TwoHundred;
    case 500:
        return LogLineCount::FiveHundred;
    }

    return std::nullopt;
}

} // namespace

LogsController::LogsController(SystemLogOperations *operations, QObject *parent)
    : QObject(parent)
    , m_operations(operations)
{
}

QString LogsController::logText() const { return m_logText; }
QString LogsController::status() const { return m_status; }
bool LogsController::busy() const { return m_busy; }
int LogsController::lineCount() const { return m_lineCount; }

void LogsController::setStatus(const QString &status)
{
    if (m_status == status)
        return;
    m_status = status;
    emit statusChanged();
}

void LogsController::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

void LogsController::setLogText(const QString &text)
{
    if (m_logText == text)
        return;
    m_logText = text;
    emit logTextChanged();
}

QString LogsController::operationErrorText(OperationStatus status, int nativeCode) const
{
    switch (status) {
    case OperationStatus::AuthorizationDenied:
        return tr("Авторизация отменена.");
    case OperationStatus::TimedOut:
        return tr("Операция превысила время ожидания.");
    case OperationStatus::Unavailable:
        return tr("Не удалось запустить helper.");
    case OperationStatus::ProtocolError:
        return tr("Получен некорректный ответ от helper.");
    case OperationStatus::Failed:
        return tr("Не удалось прочитать журнал (код %1).").arg(nativeCode);
    case OperationStatus::Succeeded:
        break;
    }

    return tr("Не удалось прочитать журнал (код %1).").arg(nativeCode);
}

void LogsController::readLogs(int lineCount)
{
    if (m_busy)
        return;

    const auto typedLineCount = logLineCountForValue(lineCount);
    if (!typedLineCount.has_value()) {
        setStatus(tr("Недопустимое количество строк журнала."));
        return;
    }
    if (!m_operations) {
        setStatus(tr("Операции с журналом недоступны."));
        return;
    }

    m_lineCount = lineCount;
    emit lineCountChanged();
    m_output.clear();
    m_outputExceeded = false;
    setBusy(true);
    setStatus(tr("Чтение журнала zapret2.service…"));

    PlatformOperation *operation = m_operations->readRecentLogs(*typedLineCount);
    if (!operation) {
        setBusy(false);
        setStatus(tr("Не удалось прочитать журнал (код %1).").arg(-1));
        return;
    }

    m_operation = operation;
    connect(operation, &PlatformOperation::standardOutputReceived, this,
            [this, operation](const QByteArray &data) {
        if (m_operation != operation || m_outputExceeded || data.isEmpty())
            return;
        if (m_output.size() + data.size() > 1024 * 1024) {
            m_outputExceeded = true;
            m_output.clear();
            return;
        }
        m_output.append(data);
    });

    connect(operation, &PlatformOperation::finished, this,
            [this, operation](const OperationResult &result) {
        if (m_operation != operation)
            return;

        m_operation = nullptr;
        setBusy(false);

        if (m_outputExceeded
            || result.diagnosticCode == QLatin1String("helper.stdout_limit_exceeded")) {
            setStatus(tr("Журнал превышает ограничение 1 MiB; данные не отображены."));
            return;
        }
        if (!result.succeeded()) {
            setStatus(operationErrorText(result.status, result.nativeCode));
            return;
        }

        const QString decoded = QString::fromUtf8(m_output);
        if (decoded.toUtf8() != m_output) {
            setStatus(tr("Журнал содержит некорректный UTF-8; данные не отображены."));
            return;
        }

        setLogText(decoded);
        setStatus(tr("Журнал обновлён: получено %1 байт.").arg(m_output.size()));
    });
}
