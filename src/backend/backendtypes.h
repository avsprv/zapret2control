#pragma once

#include <QObject>
#include <QString>

namespace Zapret::Backend {

Q_NAMESPACE

enum class EngineState {
    Unknown,
    Starting,
    Active,
    Stopping,
    Inactive,
    Failed
};
Q_ENUM_NS(EngineState)

enum class Operation {
    QueryState,
    Start,
    Stop,
    SetBootEnabled,
    ApplyConfiguration,
    CollectDiagnostics
};
Q_ENUM_NS(Operation)

enum class OperationStatus {
    Succeeded,
    Failed,
    Cancelled,
    TimedOut,
    Unsupported,
    AuthorizationDenied
};
Q_ENUM_NS(OperationStatus)

enum class Capability {
    QueryState,
    Start,
    Stop,
    BootEnablement,
    Configuration,
    Diagnostics,
    LoginAutostart
};
Q_ENUM_NS(Capability)

enum class CapabilityAvailability {
    Available,
    Unavailable,
    RequiresAuthorization
};
Q_ENUM_NS(CapabilityAvailability)

struct OperationResult {
    quint64 requestId = 0;
    Operation operation = Operation::QueryState;
    OperationStatus status = OperationStatus::Failed;
    EngineState state = EngineState::Unknown;
    QString message;

    bool succeeded() const
    {
        return status == OperationStatus::Succeeded;
    }
};

struct CapabilityInfo {
    Capability capability = Capability::QueryState;
    CapabilityAvailability availability = CapabilityAvailability::Unavailable;
    QString reason;

    bool isAvailable() const
    {
        return availability == CapabilityAvailability::Available;
    }
};

} // namespace Zapret::Backend

Q_DECLARE_METATYPE(Zapret::Backend::EngineState)
Q_DECLARE_METATYPE(Zapret::Backend::Operation)
Q_DECLARE_METATYPE(Zapret::Backend::OperationStatus)
Q_DECLARE_METATYPE(Zapret::Backend::Capability)
Q_DECLARE_METATYPE(Zapret::Backend::CapabilityAvailability)
Q_DECLARE_METATYPE(Zapret::Backend::OperationResult)
Q_DECLARE_METATYPE(Zapret::Backend::CapabilityInfo)
