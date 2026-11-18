#include "listscontroller.h"

#include <QAbstractSocket>
#include <QHostAddress>
#include <QRegularExpression>

#include <optional>

using namespace Zapret::Platform;

namespace {

const QStringList allowedNames {
    QStringLiteral("zapret-hosts-user.txt"),
    QStringLiteral("zapret-hosts-user-exclude.txt"),
    QStringLiteral("zapret-hosts-user-ipban.txt"),
    QStringLiteral("zapret-ip-user.txt"),
    QStringLiteral("zapret-ip-user6.txt"),
    QStringLiteral("zapret-ip-exclude.txt"),
    QStringLiteral("zapret-ip-exclude6.txt"),
    QStringLiteral("zapret-ip-user-ipban.txt"),
    QStringLiteral("zapret-ip-user-ipban6.txt")
};

const QStringList editableNames {
    QStringLiteral("zapret-hosts-user.txt"),
    QStringLiteral("zapret-hosts-user-exclude.txt"),
    QStringLiteral("zapret-hosts-user-ipban.txt")
};

std::optional<HostListId> listIdForName(const QString &name)
{
    if (name == QLatin1String("zapret-hosts-user.txt"))
        return HostListId::ZapretHostsUser;
    if (name == QLatin1String("zapret-hosts-user-exclude.txt"))
        return HostListId::ZapretHostsUserExclude;
    if (name == QLatin1String("zapret-hosts-user-ipban.txt"))
        return HostListId::ZapretHostsUserIpBan;
    if (name == QLatin1String("zapret-ip-user.txt"))
        return HostListId::ZapretIpUser;
    if (name == QLatin1String("zapret-ip-user6.txt"))
        return HostListId::ZapretIpUser6;
    if (name == QLatin1String("zapret-ip-exclude.txt"))
        return HostListId::ZapretIpExclude;
    if (name == QLatin1String("zapret-ip-exclude6.txt"))
        return HostListId::ZapretIpExclude6;
    if (name == QLatin1String("zapret-ip-user-ipban.txt"))
        return HostListId::ZapretIpUserIpBan;
    if (name == QLatin1String("zapret-ip-user-ipban6.txt"))
        return HostListId::ZapretIpUserIpBan6;
    return std::nullopt;
}

} // namespace

ListsController::ListsController(HostListOperations *operations, QObject *parent)
    : QObject(parent)
    , m_operations(operations)
{
}

QString ListsController::content() const { return m_content; }
QString ListsController::status() const { return m_status; }
QString ListsController::loadedName() const { return m_loadedName; }
bool ListsController::busy() const { return m_busy; }
bool ListsController::loaded() const { return m_loaded; }
bool ListsController::dirty() const { return m_dirty; }
bool ListsController::editable() const { return m_editable; }

bool ListsController::isAllowedName(const QString &name) const
{
    return allowedNames.contains(name);
}

bool ListsController::isEditableName(const QString &name) const
{
    return editableNames.contains(name);
}

void ListsController::setStatus(const QString &value)
{
    if (m_status == value)
        return;
    m_status = value;
    emit statusChanged();
}

void ListsController::setBusy(bool value)
{
    if (m_busy == value)
        return;
    m_busy = value;
    emit busyChanged();
}

void ListsController::setLoaded(bool value)
{
    if (m_loaded == value)
        return;
    m_loaded = value;
    emit loadedChanged();
}

void ListsController::setDirty(bool value)
{
    if (m_dirty == value)
        return;
    m_dirty = value;
    emit dirtyChanged();
}

void ListsController::setContentInternal(const QString &value)
{
    if (m_content == value)
        return;
    m_content = value;
    emit contentChanged();
}

void ListsController::setLoadedName(const QString &value)
{
    if (m_loadedName == value)
        return;
    m_loadedName = value;
    emit loadedNameChanged();
}

void ListsController::updateEditable()
{
    const bool value = m_loaded && isEditableName(m_loadedName);
    if (m_editable == value)
        return;
    m_editable = value;
    emit editableChanged();
}

QString ListsController::validateHostlist(const QString &value) const
{
    const QByteArray bytes = value.toUtf8();
    if (bytes.size() > 1024 * 1024)
        return tr("Файл превышает ограничение 1 MiB.");

    QStringList lines = value.split(QLatin1Char('\n'));
    if (value.endsWith(QLatin1Char('\n')) && !lines.isEmpty())
        lines.removeLast();
    if (lines.size() > 50000)
        return tr("Слишком много строк (максимум 50000).");

    static const QRegularExpression labelPattern(QStringLiteral("^[A-Za-z0-9_-]+$"));
    static const QRegularExpression numericPattern(QStringLiteral("^[0-9.]+$"));
    for (qsizetype i = 0; i < lines.size(); ++i) {
        const QString raw = lines.at(i);
        if (raw.toUtf8().size() > 4095)
            return tr("Строка %1 длиннее 4095 байт.").arg(i + 1);
        for (const QChar ch : raw) {
            if (ch.category() == QChar::Other_Control && ch != QLatin1Char('\r'))
                return tr("Строка %1 содержит управляющий символ.").arg(i + 1);
        }

        QString line = raw;
        while (line.startsWith(QLatin1Char(' ')) || line.startsWith(QLatin1Char('\t')))
            line.remove(0, 1);
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))
            || line.startsWith(QLatin1Char(';')) || line.startsWith(QLatin1Char('/'))) {
            continue;
        }

        while (line.startsWith(QLatin1Char('^')) || line.startsWith(QLatin1Char('!')))
            line.remove(0, 1);
        const qsizetype tokenEnd = line.indexOf(QRegularExpression(QStringLiteral("[\\s]")));
        const QString token = tokenEnd < 0 ? line : line.left(tokenEnd);
        if (token.isEmpty())
            return tr("Строка %1 содержит флаги без записи.").arg(i + 1);
        if (QString::fromLatin1(token.toLatin1()) != token)
            return tr("Строка %1: используйте ASCII-домен или punycode.").arg(i + 1);

        QHostAddress address;
        if (address.setAddress(token))
            continue;
        const auto subnet = QHostAddress::parseSubnet(token);
        if (!subnet.first.isNull() && subnet.second >= 0
            && subnet.second <= (subnet.first.protocol() == QAbstractSocket::IPv4Protocol ? 32 : 128)) {
            continue;
        }
        if (token.contains(QLatin1Char('/')) || token.contains(QLatin1Char(':'))
            || numericPattern.match(token).hasMatch()) {
            return tr("Некорректный IP/CIDR в строке %1: %2").arg(i + 1).arg(token);
        }

        QString domain = token;
        if (domain.endsWith(QLatin1Char('.')))
            domain.chop(1);
        if (domain.isEmpty() || domain.size() > 253)
            return tr("Домен в строке %1 имеет недопустимую длину.").arg(i + 1);
        for (const QString &label : domain.split(QLatin1Char('.'))) {
            if (label.isEmpty() || label.size() > 63 || !labelPattern.match(label).hasMatch()
                || label.startsWith(QLatin1Char('-')) || label.endsWith(QLatin1Char('-'))) {
                return tr("Некорректный домен или IP/CIDR в строке %1: %2").arg(i + 1).arg(token);
            }
        }
    }

    return {};
}

QString ListsController::operationErrorText(OperationStatus status, bool writing) const
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
    case OperationStatus::Succeeded:
        break;
    case OperationStatus::Failed:
        break;
    }

    return writing ? tr("Не удалось сохранить список.") : tr("Не удалось прочитать список.");
}

void ListsController::readList(const QString &name)
{
    if (m_busy)
        return;

    const auto listId = listIdForName(name);
    if (!listId.has_value() || !isAllowedName(name)) {
        setStatus(tr("Недопустимое имя списка."));
        return;
    }
    if (!m_operations) {
        setStatus(tr("Операции со списками недоступны."));
        return;
    }

    m_output.clear();
    m_outputExceeded = false;
    setContentInternal(QString());
    setLoaded(false);
    setDirty(false);
    setLoadedName(QString());
    updateEditable();
    setBusy(true);
    setStatus(tr("Чтение списка %1…").arg(name));

    PlatformOperation *operation = m_operations->readList(*listId);
    if (!operation) {
        setBusy(false);
        setStatus(tr("Не удалось прочитать список."));
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
            [this, operation, name](const OperationResult &result) {
        if (m_operation != operation)
            return;

        m_operation = nullptr;
        setBusy(false);

        if (m_outputExceeded
            || result.diagnosticCode == QLatin1String("helper.stdout_limit_exceeded")) {
            setStatus(tr("Список превышает ограничение 1 MiB; данные не отображены."));
            return;
        }
        if (!result.succeeded()) {
            setStatus(operationErrorText(result.status, false));
            return;
        }

        if (result.diagnosticCode == QLatin1String("hostlist.not_found")) {
            setLoadedName(name);
            setContentInternal(QString());
            setLoaded(true);
            setDirty(false);
            updateEditable();
            setStatus(tr("Файл %1 ещё не создан. Можно добавить записи и сохранить его.").arg(name));
            return;
        }

        const QString decoded = QString::fromUtf8(m_output);
        if (decoded.toUtf8() != m_output) {
            setStatus(tr("Список содержит некорректный UTF-8; данные не отображены."));
            return;
        }

        setLoadedName(name);
        setContentInternal(decoded);
        setLoaded(true);
        setDirty(false);
        updateEditable();
        setStatus(tr("Список %1 прочитан.").arg(name));
    });
}

void ListsController::setContent(const QString &value)
{
    if (m_busy || !m_loaded || !m_editable || m_content == value)
        return;
    setContentInternal(value);
    setDirty(true);
}

void ListsController::saveList()
{
    if (m_busy || !m_loaded || !m_dirty || !m_editable)
        return;

    const QString validation = validateHostlist(m_content);
    if (!validation.isEmpty()) {
        setStatus(validation);
        return;
    }
    if (!m_operations) {
        setStatus(tr("Операции со списками недоступны."));
        return;
    }

    const auto listId = listIdForName(m_loadedName);
    if (!listId.has_value() || !isEditableName(m_loadedName)) {
        setStatus(tr("Недопустимое имя списка."));
        return;
    }

    setBusy(true);
    setStatus(tr("Проверка и сохранение списка %1…").arg(m_loadedName));

    PlatformOperation *operation = m_operations->writeList(*listId, m_content.toUtf8());
    if (!operation) {
        setBusy(false);
        setStatus(tr("Не удалось сохранить список."));
        return;
    }

    m_operation = operation;
    connect(operation, &PlatformOperation::finished, this,
            [this, operation](const OperationResult &result) {
        if (m_operation != operation)
            return;

        m_operation = nullptr;
        setBusy(false);
        if (result.succeeded()) {
            setDirty(false);
            setStatus(tr("Список сохранён."));
        } else {
            setStatus(operationErrorText(result.status, true));
        }
    });
}

