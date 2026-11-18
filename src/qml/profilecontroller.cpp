#include "profilecontroller.h"

#include <QCryptographicHash>

using namespace Zapret::Platform;

namespace {
constexpr qsizetype ConfigurationLimit = 1024 * 1024;
}

ProfileController::ProfileController(ConfigurationOperations *operations, QObject *parent)
    : QObject(parent)
    , m_operations(operations)
{
    Q_ASSERT(m_operations);
}

QVariantList ProfileController::profiles() const
{
    QVariantList result;
    for (int i = 0; i < m_profiles.size(); ++i) {
        const auto &profile = m_profiles.at(i);
        result.append(QVariantMap{
            {QStringLiteral("index"), i},
            {QStringLiteral("name"), tr("Профиль %1").arg(i + 1)},
            {QStringLiteral("arguments"), profile.allArguments},
            {QStringLiteral("filters"), profile.filters.size()},
            {QStringLiteral("payloads"), profile.payloads.size()},
            {QStringLiteral("luaDesync"), profile.luaDesync.size()},
            {QStringLiteral("other"), profile.other.size()}
        });
    }
    return result;
}

int ProfileController::selectedIndex() const { return m_selectedIndex; }

void ProfileController::setSelectedIndex(int index)
{
    if (index < -1 || index >= m_profiles.size() || index == m_selectedIndex)
        return;
    m_selectedIndex = index;
    emit selectedIndexChanged();
}

QString ProfileController::status() const { return m_status; }
bool ProfileController::busy() const { return m_busy; }
bool ProfileController::loaded() const { return m_loaded; }
bool ProfileController::dirty() const { return m_dirty; }

void ProfileController::setStatus(const QString &value)
{
    if (m_status == value)
        return;
    m_status = value;
    emit statusChanged();
}

void ProfileController::setBusy(bool value)
{
    if (m_busy == value)
        return;
    m_busy = value;
    emit busyChanged();
}

void ProfileController::setDirty(bool value)
{
    if (m_dirty == value)
        return;
    m_dirty = value;
    emit dirtyChanged();
}

void ProfileController::setLoaded(bool value)
{
    if (m_loaded == value)
        return;
    m_loaded = value;
    emit loadedChanged();
}

bool ProfileController::validArguments(const QStringList &arguments, QString *error) const
{
    if (arguments.isEmpty()) {
        *error = tr("Профиль должен содержать хотя бы один аргумент.");
        return false;
    }
    if (arguments.size() > 4096) {
        *error = tr("Слишком много аргументов.");
        return false;
    }
    for (const QString &argument : arguments) {
        if (argument.isEmpty() || argument == QStringLiteral("--new")) {
            *error = tr("Пустой аргумент или зарезервированный разделитель --new.");
            return false;
        }
        for (const QChar character : argument) {
            if (character.isSpace() || character.category() == QChar::Other_Control
                || character == QLatin1Char('"') || character == QLatin1Char('$')
                || character == QChar(96) || character == QLatin1Char('\\')) {
                *error = tr("Аргумент содержит пробелы или небезопасные shell-символы.");
                return false;
            }
        }
    }
    return true;
}

void ProfileController::startOperation(PlatformOperation *operation, bool writing,
                                       const QByteArray &input)
{
    if (!operation) {
        setBusy(false);
        setStatus(tr("Не удалось запустить helper: %1").arg(QStringLiteral("unavailable")));
        return;
    }

    m_operation = operation;
    m_output.clear();
    m_configInput = input;

    connect(operation, &PlatformOperation::standardOutputReceived, this,
            [this, operation](const QByteArray &data) {
        if (m_operation != operation)
            return;
        if (m_output.size() + data.size() > ConfigurationLimit) {
            m_output.clear();
            setStatus(tr("Конфигурация превышает 1 MiB."));
            setBusy(false);
            m_operation = nullptr;
            return;
        }
        m_output.append(data);
    });
    connect(operation, &PlatformOperation::finished, this,
            [this, operation, writing](const OperationResult &result) {
        finishOperation(operation, writing, result);
    });
}

void ProfileController::finishOperation(PlatformOperation *operation, bool writing,
                                        const OperationResult &result)
{
    if (m_operation != operation)
        return;

    const QByteArray output = m_output;
    const QByteArray input = m_configInput;
    m_operation = nullptr;
    m_output.clear();
    m_configInput.clear();
    setBusy(false);

    if (!result.succeeded()) {
        if (writing)
            setStatus(tr("Запись не выполнена (код %1).").arg(result.nativeCode));
        else
            setStatus(tr("Не удалось прочитать конфигурацию (код %1).").arg(result.nativeCode));
        return;
    }

    if (writing) {
        m_configBytes = input;
        m_config = QString::fromUtf8(input);
        setDirty(false);
        emit profilesChanged();
        setStatus(tr("Профили сохранены. Резервная копия создана; служба не перезапускалась."));
        return;
    }

    const QString decoded = QString::fromUtf8(output);
    if (decoded.toUtf8() != output) {
        setStatus(tr("Конфигурация содержит некорректный UTF-8; анализ отменён."));
        return;
    }

    const auto parsed = ProfileAnalyzer::parse(decoded);
    if (!parsed.ok()) {
        setStatus(parsed.error);
        return;
    }

    m_configBytes = output;
    m_config = decoded;
    m_profiles = parsed.profiles;
    setSelectedIndex(-1);
    setLoaded(true);
    setDirty(false);
    emit profilesChanged();
    setStatus(tr("Загружено профилей: %1. Конфигурация не исполнялась.").arg(m_profiles.size()));
}

void ProfileController::read(bool discardChanges)
{
    if (m_busy)
        return;
    if (m_dirty && !discardChanges) {
        setStatus(tr("Есть несохранённые изменения. Сохраните их или подтвердите отмену."));
        return;
    }
    setBusy(true);
    setStatus(tr("Чтение конфигурации…"));
    startOperation(m_operations->readConfiguration(), false);
}

void ProfileController::add(const QStringList &arguments)
{
    if (!m_loaded || m_busy || m_profiles.size() >= 128)
        return;
    QString error;
    if (!validArguments(arguments, &error)) {
        setStatus(error);
        return;
    }
    auto candidate = m_profiles;
    ProfileAnalyzer::Profile profile;
    profile.allArguments = arguments;
    for (const auto &argument : arguments) {
        if (argument.startsWith("--filter-"))
            profile.filters.append(argument);
        else if (argument.startsWith("--payload="))
            profile.payloads.append(argument);
        else if (argument.startsWith("--lua-desync="))
            profile.luaDesync.append(argument);
        else
            profile.other.append(argument);
    }
    candidate.append(profile);
    if (!ProfileAnalyzer::replaceProfiles(m_config, candidate).ok()) {
        setStatus(tr("Изменения не прошли проверку анализатора."));
        return;
    }
    m_profiles = candidate;
    setSelectedIndex(m_profiles.size() - 1);
    setDirty(true);
    emit profilesChanged();
    setStatus(tr("Профиль добавлен. Изменения ещё не сохранены."));
}

void ProfileController::edit(int index, const QStringList &arguments)
{
    if (!m_loaded || m_busy || index < 0 || index >= m_profiles.size())
        return;
    QString error;
    if (!validArguments(arguments, &error)) {
        setStatus(error);
        return;
    }
    auto candidate = m_profiles;
    auto &profile = candidate[index];
    profile = ProfileAnalyzer::Profile{};
    profile.allArguments = arguments;
    for (const auto &argument : arguments) {
        if (argument.startsWith("--filter-"))
            profile.filters.append(argument);
        else if (argument.startsWith("--payload="))
            profile.payloads.append(argument);
        else if (argument.startsWith("--lua-desync="))
            profile.luaDesync.append(argument);
        else
            profile.other.append(argument);
    }
    if (!ProfileAnalyzer::replaceProfiles(m_config, candidate).ok()) {
        setStatus(tr("Изменения не прошли проверку анализатора."));
        return;
    }
    m_profiles = candidate;
    setSelectedIndex(index);
    setDirty(true);
    emit profilesChanged();
    setStatus(tr("Профиль изменён. Изменения ещё не сохранены."));
}

void ProfileController::copy(int index)
{
    if (!m_loaded || m_busy || index < 0 || index >= m_profiles.size()
        || m_profiles.size() >= 128)
        return;
    m_profiles.insert(index + 1, m_profiles.at(index));
    setSelectedIndex(index + 1);
    setDirty(true);
    emit profilesChanged();
    setStatus(tr("Профиль скопирован. Изменения ещё не сохранены."));
}

void ProfileController::remove(int index)
{
    if (!m_loaded || m_busy || index < 0 || index >= m_profiles.size()
        || m_profiles.size() <= 1)
        return;
    const int previousSelection = m_selectedIndex;
    m_profiles.removeAt(index);
    int nextSelection = previousSelection;
    if (previousSelection == index)
        nextSelection = qMin(index, m_profiles.size() - 1);
    else if (previousSelection > index)
        nextSelection = previousSelection - 1;
    setSelectedIndex(nextSelection);
    setDirty(true);
    emit profilesChanged();
    setStatus(tr("Профиль удалён. Изменения ещё не сохранены."));
}

void ProfileController::save()
{
    if (!m_loaded || m_busy || !m_dirty)
        return;
    const auto edited = ProfileAnalyzer::replaceProfiles(m_config, m_profiles);
    if (!edited.ok()) {
        setStatus(edited.error);
        return;
    }
    const QByteArray bytes = edited.config.toUtf8();
    if (bytes.size() > ConfigurationLimit) {
        setStatus(tr("Конфигурация превышает 1 MiB."));
        return;
    }

    const QByteArray digestHex =
        QCryptographicHash::hash(m_configBytes, QCryptographicHash::Sha256).toHex();
    const auto digest = Sha256Digest::fromHex(digestHex);
    if (!digest) {
        setStatus(tr("Запись не выполнена (код %1).").arg(-1));
        return;
    }

    setBusy(true);
    setStatus(tr("Проверка и безопасная запись профилей…"));
    startOperation(m_operations->writeConfiguration(bytes, *digest), true, bytes);
}
