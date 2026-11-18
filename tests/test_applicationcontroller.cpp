#include <QtTest>
#include <QPointer>

#include "qml/applicationcontroller.h"
#include "backend/backendtypes.h"

class FakeBackend final : public ZapretBackend
{
    Q_OBJECT

public:
    using ZapretBackend::ZapretBackend;

    int refreshCount = 0;
    int startCount = 0;
    int stopCount = 0;
    int bootChangeCount = 0;
    bool deferActions = false;
    quint64 lastRequestId = 0;

    void completeRequest(quint64 requestId, Zapret::Backend::Operation operation,
                         ZapretBackend::State state,
                         Zapret::Backend::OperationStatus status =
                             Zapret::Backend::OperationStatus::Succeeded,
                         const QString &message = {})
    {
        using Zapret::Backend::EngineState;

        EngineState engineState = EngineState::Unknown;
        switch (state) {
        case ZapretBackend::State::Active:
            engineState = EngineState::Active;
            break;
        case ZapretBackend::State::Inactive:
            engineState = EngineState::Inactive;
            break;
        case ZapretBackend::State::Failed:
            engineState = EngineState::Failed;
            break;
        case ZapretBackend::State::Unknown:
        default:
            break;
        }

        emit operationFinished({requestId, operation, status, engineState, message});
    }

    void refresh() override
    {
        ++refreshCount;
    }

    void verifyState() override
    {
    }

    void verifyState(quint64 requestId) override
    {
        emit stateVerifiedForRequest(requestId, ZapretBackend::State::Unknown);
    }

    void start() override
    {
        ++startCount;
        emit unitActionFinished(QStringLiteral("start"), ZapretBackend::State::Active, true);
    }

    void start(quint64 requestId) override
    {
        ++startCount;
        lastRequestId = requestId;
        if (!deferActions) {
            completeRequest(requestId, Zapret::Backend::Operation::Start,
                            ZapretBackend::State::Active);
            emit unitActionFinishedForRequest(requestId, QStringLiteral("start"),
                                              ZapretBackend::State::Active, true);
        }
    }

    void stop() override
    {
        ++stopCount;
        emit unitActionFinished(QStringLiteral("stop"), ZapretBackend::State::Inactive, true);
    }

    void stop(quint64 requestId) override
    {
        ++stopCount;
        lastRequestId = requestId;
        if (!deferActions) {
            completeRequest(requestId, Zapret::Backend::Operation::Stop,
                            ZapretBackend::State::Inactive);
            emit unitActionFinishedForRequest(requestId, QStringLiteral("stop"),
                                              ZapretBackend::State::Inactive, true);
        }
    }

    void setBootEnabled(bool enabled) override
    {
        ++bootChangeCount;
        emit bootEnabledChanged(enabled);
        emit unitActionFinished(enabled ? QStringLiteral("enable") : QStringLiteral("disable"),
                                ZapretBackend::State::Unknown, true);
    }

    void setBootEnabled(bool enabled, quint64 requestId) override
    {
        ++bootChangeCount;
        lastRequestId = requestId;
        if (!deferActions) {
            emit bootEnabledChanged(enabled);
            completeRequest(requestId, Zapret::Backend::Operation::SetBootEnabled,
                            ZapretBackend::State::Unknown);
            emit unitActionFinishedForRequest(
                requestId, enabled ? QStringLiteral("enable") : QStringLiteral("disable"),
                ZapretBackend::State::Unknown, true);
        }
    }
};

class ApplicationControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void injectedBackendReceivesControllerCommands();
    void ignoresCompletionForUnrelatedRequest();
    void controllerOwnsInjectedBackend();
    void typedOperationResultReportsOutcome();
    void capabilityAvailabilityIsExplicit();
    void controllerConsumesTypedOperationFailure();
};

void ApplicationControllerTest::injectedBackendReceivesControllerCommands()
{
    auto *backend = new FakeBackend;
    ApplicationController controller(backend, nullptr);

    QCOMPARE(backend->refreshCount, 1);

    controller.refresh();
    QCOMPARE(backend->refreshCount, 2);

    controller.startService();
    QCOMPARE(backend->startCount, 1);
    QVERIFY(controller.serviceActive());
    QCOMPARE(controller.serviceStateCode(), QStringLiteral("active"));
    QVERIFY(!controller.operationsBusy());

    controller.stopService();
    QCOMPARE(backend->stopCount, 1);
    QVERIFY(!controller.serviceActive());
    QCOMPARE(controller.serviceStateCode(), QStringLiteral("inactive"));
    QVERIFY(!controller.operationsBusy());

    controller.setBootEnabled(true);
    QCOMPARE(backend->bootChangeCount, 1);
    QVERIFY(controller.bootEnabled());
    QVERIFY(!controller.operationsBusy());
}

void ApplicationControllerTest::ignoresCompletionForUnrelatedRequest()
{
    auto *backend = new FakeBackend;
    backend->deferActions = true;
    ApplicationController controller(backend, nullptr);

    controller.startService();
    const quint64 requestId = backend->lastRequestId;
    QVERIFY(requestId != 0);
    QVERIFY(controller.operationsBusy());

    backend->completeRequest(requestId + 1, Zapret::Backend::Operation::Start,
                             ZapretBackend::State::Active);
    QVERIFY(controller.operationsBusy());
    QVERIFY(!controller.serviceActive());

    backend->completeRequest(requestId, Zapret::Backend::Operation::Start,
                             ZapretBackend::State::Active);
    QVERIFY(!controller.operationsBusy());
    QVERIFY(controller.serviceActive());
}

void ApplicationControllerTest::controllerOwnsInjectedBackend()
{
    auto *backend = new FakeBackend;
    QPointer<FakeBackend> backendGuard(backend);

    {
        ApplicationController controller(backend, nullptr);
        QVERIFY(!backendGuard.isNull());
    }

    QVERIFY(backendGuard.isNull());
}


void ApplicationControllerTest::typedOperationResultReportsOutcome()
{
    using namespace Zapret::Backend;

    const OperationResult result{
        17,
        Operation::Start,
        OperationStatus::AuthorizationDenied,
        EngineState::Inactive,
        QStringLiteral("Authorization was denied")
    };

    QCOMPARE(result.requestId, quint64(17));
    QCOMPARE(result.operation, Operation::Start);
    QCOMPARE(result.status, OperationStatus::AuthorizationDenied);
    QCOMPARE(result.state, EngineState::Inactive);
    QVERIFY(!result.succeeded());

    const OperationResult success{
        18,
        Operation::Stop,
        OperationStatus::Succeeded,
        EngineState::Inactive,
        {}
    };
    QVERIFY(success.succeeded());
}

void ApplicationControllerTest::capabilityAvailabilityIsExplicit()
{
    using namespace Zapret::Backend;

    const CapabilityInfo supported{
        Capability::Start,
        CapabilityAvailability::Available,
        {}
    };
    const CapabilityInfo privileged{
        Capability::BootEnablement,
        CapabilityAvailability::RequiresAuthorization,
        QStringLiteral("Requires elevated authorization")
    };
    const CapabilityInfo unavailable{
        Capability::LoginAutostart,
        CapabilityAvailability::Unavailable,
        QStringLiteral("Not implemented on this platform")
    };

    QVERIFY(supported.isAvailable());
    QVERIFY(!privileged.isAvailable());
    QVERIFY(!unavailable.isAvailable());
    QCOMPARE(privileged.reason, QStringLiteral("Requires elevated authorization"));
    QCOMPARE(unavailable.capability, Capability::LoginAutostart);
}

void ApplicationControllerTest::controllerConsumesTypedOperationFailure()
{
    auto *backend = new FakeBackend;
    backend->deferActions = true;
    ApplicationController controller(backend, nullptr);

    controller.startService();
    const quint64 requestId = backend->lastRequestId;
    QVERIFY(controller.operationsBusy());

    backend->completeRequest(requestId, Zapret::Backend::Operation::Start,
                             ZapretBackend::State::Inactive,
                             Zapret::Backend::OperationStatus::AuthorizationDenied,
                             QStringLiteral("Authorization was denied"));

    QVERIFY(!controller.operationsBusy());
    QVERIFY(!controller.serviceActive());
    QCOMPARE(controller.errorMessage(), QStringLiteral("Authorization was denied"));
}

QTEST_GUILESS_MAIN(ApplicationControllerTest)
#include "test_applicationcontroller.moc"
