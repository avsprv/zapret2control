#include "configcontroller.h"

#include <QCryptographicHash>
#include <QFileInfo>
#include <QProcess>
#include <QSaveFile>
#include <QTemporaryFile>

using namespace Zapret::Platform;

ConfigController::ConfigController(ConfigurationOperations *operations, QObject *parent)
    : QObject(parent)
    , m_operations(operations)
{
    Q_ASSERT(m_operations);
}

QString ConfigController::configText() const { return m_configText; }

void ConfigController::setConfigText(const QString &text)
{
    if (m_configText == text)
        return;
    m_configText = text;
    emit configTextChanged();
}

QString ConfigController::status() const { return m_status; }
bool ConfigController::busy() const { return m_busy; }
bool ConfigController::loaded() const { return m_loaded; }

void ConfigController::setStatus(const QString &status)
{
    if (m_status == status)
        return;
    m_status = status;
    emit statusChanged();
}

void ConfigController::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

void ConfigController::startOperation(PlatformOperation *operation, OperationKind kind,
                                      const QByteArray &input)
{
    if (!operation) {
        setBusy(false);
        setStatus(tr("Операция недоступна на этой платформе."));
        return;
    }

    m_operation = operation;
    m_operationOutput.clear();
    m_operationError.clear();
    m_pendingInput = input;

    connect(operation, &PlatformOperation::standardOutputReceived, this,
            [this, operation](const QByteArray &data) {
        if (m_operation == operation)
            m_operationOutput.append(data);
    });
    connect(operation, &PlatformOperation::standardErrorReceived, this,
            [this, operation](const QByteArray &data) {
        if (m_operation == operation)
            m_operationError.append(data);
    });
    connect(operation, &PlatformOperation::finished, this,
            [this, operation, kind](const OperationResult &result) {
        handleOperationFinished(operation, kind, result);
    });
}

void ConfigController::handleOperationFinished(PlatformOperation *operation,
                                                OperationKind kind,
                                                const OperationResult &result)
{
    if (m_operation != operation)
        return;

    const QByteArray output = m_operationOutput;
    const QString error = QString::fromUtf8(m_operationError).trimmed();
    const QByteArray input = m_pendingInput;
    m_operation = nullptr;
    m_operationOutput.clear();
    m_operationError.clear();
    m_pendingInput.clear();
    setBusy(false);

    if (!result.succeeded()) {
        if (!error.isEmpty()) {
            setStatus(error);
        } else if (result.status == OperationStatus::AuthorizationDenied) {
            setStatus(tr("Операция отменена: авторизация не выполнена."));
        } else if (result.status == OperationStatus::TimedOut) {
            setStatus(tr("Операция не завершилась за отведённое время."));
        } else if (result.status == OperationStatus::Unavailable) {
            setStatus(tr("Привилегированный helper недоступен."));
        } else {
            setStatus(tr("Операция не выполнена (код %1).").arg(result.nativeCode));
        }
        return;
    }

    if (kind == OperationKind::Read) {
        const QString decoded = QString::fromUtf8(output);
        if (decoded.toUtf8() != output) {
            setStatus(tr("Файл конфигурации содержит некорректную последовательность UTF-8; чтение отменено во избежание потери данных."));
            return;
        }

        m_originalBytes = output;
        setConfigText(decoded);
        if (!m_loaded) {
            m_loaded = true;
            emit loadedChanged();
        }
        setStatus(tr("Конфигурация загружена. Перед сохранением будет проверен синтаксис и создана резервная копия."));
    } else if (kind == OperationKind::Save) {
        m_originalBytes = input;
        setStatus(QString::fromUtf8(output).trimmed());
    } else {
        setStatus(QString::fromUtf8(output).trimmed());
        read();
    }
}

void ConfigController::read()
{
    if (m_busy)
        return;
    setBusy(true);
    setStatus(tr("Чтение /opt/zapret2/config…"));
    startOperation(m_operations->readConfiguration(), OperationKind::Read);
}

void ConfigController::validate()
{
    if (!m_loaded || m_busy)
        return;

    const QByteArray bytes = m_configText.toUtf8();
    if (bytes.size() > 1024 * 1024) {
        setStatus(tr("Конфигурация превышает ограничение 1 MiB."));
        return;
    }

    QTemporaryFile staged;
    if (!staged.open() || staged.write(bytes) != bytes.size() || !staged.flush()) {
        setStatus(tr("Не удалось подготовить файл для проверки: %1").arg(staged.errorString()));
        return;
    }

    QProcess validator;
    validator.start(QStringLiteral("/bin/sh"), {QStringLiteral("-n"), staged.fileName()});
    if (!validator.waitForStarted(3000) || !validator.waitForFinished(10000)) {
        validator.kill();
        validator.waitForFinished();
        setStatus(tr("Проверка shell-синтаксиса не завершилась."));
    } else if (validator.exitStatus() == QProcess::NormalExit && validator.exitCode() == 0) {
        setStatus(tr("Синтаксис корректен. Файл не исполнялся; системная конфигурация не изменена."));
    } else {
        setStatus(tr("Ошибка shell-синтаксиса: %1").arg(QString::fromUtf8(validator.readAllStandardError()).trimmed()));
    }
}

void ConfigController::save()
{
    if (!m_loaded || m_busy)
        return;

    const QByteArray bytes = m_configText.toUtf8();
    if (bytes.size() > 1024 * 1024) {
        setStatus(tr("Конфигурация превышает ограничение 1 MiB."));
        return;
    }

    QTemporaryFile staged;
    if (!staged.open() || staged.write(bytes) != bytes.size() || !staged.flush()) {
        setStatus(tr("Не удалось подготовить файл для проверки: %1").arg(staged.errorString()));
        return;
    }

    QProcess validator;
    validator.start(QStringLiteral("/bin/sh"), {QStringLiteral("-n"), staged.fileName()});
    if (!validator.waitForStarted(3000) || !validator.waitForFinished(10000)
        || validator.exitStatus() != QProcess::NormalExit || validator.exitCode() != 0) {
        setStatus(tr("Сохранение отменено: shell-синтаксис не прошёл проверку."));
        return;
    }

    const QByteArray digestHex =
        QCryptographicHash::hash(m_originalBytes, QCryptographicHash::Sha256).toHex();
    const auto digest = Sha256Digest::fromHex(digestHex);
    if (!digest) {
        setStatus(tr("Не удалось вычислить контрольную сумму исходной конфигурации."));
        return;
    }

    setBusy(true);
    setStatus(tr("Helper повторно проверит синтаксис и контрольную сумму, создаст резервную копию и атомарно сохранит файл…"));
    startOperation(m_operations->writeConfiguration(bytes, *digest),
                   OperationKind::Save, bytes);
}

bool ConfigController::exportConfig(const QUrl &url)
{
    if (!m_loaded) {
        setStatus(tr("Экспорт доступен после чтения конфигурации."));
        return false;
    }
    if (m_busy)
        return false;
    if (!url.isLocalFile()) {
        setStatus(tr("Не удалось сохранить копию: недопустимый путь к локальному файлу."));
        return false;
    }

    const QString path = url.toLocalFile();
    const QByteArray bytes = m_configText.toUtf8();
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(bytes) != bytes.size()
        || !file.commit()) {
        setStatus(tr("Не удалось сохранить копию: %1").arg(file.errorString()));
        return false;
    }

    setStatus(tr("Копия конфигурации сохранена: %1").arg(QFileInfo(path).fileName()));
    return true;
}

void ConfigController::rollback()
{
    if (!m_loaded || m_busy)
        return;
    setBusy(true);
    setStatus(tr("Восстановление последней резервной копии…"));
    startOperation(m_operations->rollbackConfiguration(), OperationKind::Rollback);
}
