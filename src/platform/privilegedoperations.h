#pragma once

#include <QByteArray>
#include <QRegularExpression>
#include <QStringList>
#include <QUuid>

#include <optional>
#include <utility>

#include "platform/privilegedoperation.h"

namespace Zapret::Platform {

enum class HostListId {
    ZapretHostsUser,
    ZapretHostsUserExclude,
    ZapretHostsUserIpBan,
    ZapretIpUser,
    ZapretIpUser6,
    ZapretIpExclude,
    ZapretIpExclude6,
    ZapretIpUserIpBan,
    ZapretIpUserIpBan6
};

enum class LogLineCount : int {
    Fifty = 50,
    OneHundred = 100,
    TwoHundred = 200,
    FiveHundred = 500
};

enum class BlockcheckIpVersion {
    IPv4,
    IPv6,
    DualStack
};

enum class BlockcheckScanLevel {
    VeryQuick,
    Quick,
    Standard,
    Force
};

struct BlockcheckOptions {
    QStringList domains{QStringLiteral("rutracker.org")};
    BlockcheckIpVersion ipVersion = BlockcheckIpVersion::IPv4;
    bool http = true;
    bool tls12 = true;
    bool tls13 = false;
    bool http3 = false;
    int repeats = 1;
    BlockcheckScanLevel scanLevel = BlockcheckScanLevel::Standard;
};

struct OperationUuid {
    QUuid value;

    bool isValid() const
    {
        return !value.isNull();
    }
};

struct StartEpoch {
    qint64 seconds = 0;

    bool isValid() const
    {
        return seconds >= 0;
    }
};

class Sha256Digest final
{
public:
    static std::optional<Sha256Digest> fromHex(const QByteArray &value)
    {
        if (value.size() != 64)
            return std::nullopt;

        const QByteArray decoded = QByteArray::fromHex(value);
        if (decoded.size() != 32 || decoded.toHex() != value)
            return std::nullopt;

        return Sha256Digest(value);
    }

    const QByteArray &hex() const
    {
        return m_hex;
    }

private:
    explicit Sha256Digest(QByteArray value)
        : m_hex(std::move(value))
    {
    }

    QByteArray m_hex;
};

class ValidatedUpstreamVersion final
{
public:
    static std::optional<ValidatedUpstreamVersion> fromTag(const QString &tag)
    {
        static const QRegularExpression pattern(
            QStringLiteral(R"(^v[0-9]+(?:\.[0-9]+)*$)"));
        if (tag.size() > 24 || !pattern.match(tag).hasMatch())
            return std::nullopt;

        return ValidatedUpstreamVersion(tag);
    }

    const QString &tag() const
    {
        return m_tag;
    }

private:
    explicit ValidatedUpstreamVersion(QString tag)
        : m_tag(std::move(tag))
    {
    }

    QString m_tag;
};

// All returned operation handles are owned by the provider implementation.
// Consumers may observe them but must not delete them.
class ConfigurationOperations
{
public:
    virtual ~ConfigurationOperations() = default;

    virtual PlatformOperation *readConfiguration() = 0;
    virtual PlatformOperation *writeConfiguration(
        const QByteArray &contents, const Sha256Digest &expectedDigest) = 0;
    virtual PlatformOperation *rollbackConfiguration() = 0;
};

class HostListOperations
{
public:
    virtual ~HostListOperations() = default;

    virtual PlatformOperation *readList(HostListId listId) = 0;
    virtual PlatformOperation *writeList(HostListId listId, const QByteArray &contents) = 0;
};

class SystemLogOperations
{
public:
    virtual ~SystemLogOperations() = default;

    virtual PlatformOperation *readRecentLogs(LogLineCount lineCount) = 0;
};

class ServiceControlOperations
{
public:
    virtual ~ServiceControlOperations() = default;

    virtual PlatformOperation *queryEngineState() = 0;
    virtual PlatformOperation *queryBootEnabled() = 0;
    virtual PlatformOperation *startEngine() = 0;
    virtual PlatformOperation *stopEngine() = 0;
    virtual PlatformOperation *setBootEnabled(bool enabled) = 0;
};

class BlockcheckOperations
{
public:
    virtual ~BlockcheckOperations() = default;

    virtual PlatformOperation *runDiagnostic(const BlockcheckOptions &options) = 0;
    virtual PlatformOperation *cancelDiagnostic() = 0;
    virtual PlatformOperation *submitDiagnosticJob(const BlockcheckOptions &options) = 0;
    virtual PlatformOperation *probeDiagnosticRecovery() = 0;
    virtual PlatformOperation *queryDiagnosticJob(const OperationUuid &operationId) = 0;
    virtual PlatformOperation *readDiagnosticJobLogs(
        LogLineCount lineCount, const StartEpoch &startEpoch) = 0;
    virtual PlatformOperation *cancelDiagnosticJob(const OperationUuid &operationId) = 0;
    virtual PlatformOperation *retryDiagnosticRecovery() = 0;
};

class UpstreamMaintenanceOperations
{
public:
    virtual ~UpstreamMaintenanceOperations() = default;

    virtual PlatformOperation *installUpstreamVersion(
        const ValidatedUpstreamVersion &version) = 0;
    virtual PlatformOperation *updateUpstreamVersion(
        const ValidatedUpstreamVersion &version) = 0;
};

} // namespace Zapret::Platform

Q_DECLARE_METATYPE(Zapret::Platform::HostListId)
Q_DECLARE_METATYPE(Zapret::Platform::LogLineCount)
Q_DECLARE_METATYPE(Zapret::Platform::BlockcheckOptions)
Q_DECLARE_METATYPE(Zapret::Platform::OperationUuid)
Q_DECLARE_METATYPE(Zapret::Platform::StartEpoch)
