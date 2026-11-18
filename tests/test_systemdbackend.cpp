#include <QtTest>

#include <QPointer>
#include <QTimer>

#include "backend/systemdbackend.h"

using namespace Zapret::Platform;

class FakeServiceControlOperations final : public ServiceControlOperations
{
    Q_OBJECT

public:
    using ServiceControlOperations::ServiceControlOperations;

    QStringList actions;
    int activePrivilegedProcesses = 0;
    int maxConcurrentPrivilegedProcesses = 0;
    bool failNextActionToStart = false;
    bool failNextActionToExecute = false;
    int nextActionExitCode = -1;
    bool failNextStateQueryToStart = false;
    int activeQueryCount = 0;
    bool holdPrivilegedProcessOpen = false;
    bool delayFirstStateQuery = false;
    bool delayFirstBootQuery = false;
    bool delayNextAction = false;
    bool delayNextStateQuery = false;
    bool delayNextStateQueryBriefly = false;
    int bootQueryCount = 0;
    QPointer<PlatformOperation> lastPrivilegedOperation;

    PlatformOperation *queryEngineState() override
    {
        if (failNextStateQueryToStart) {
            failNextStateQueryToStart = false;
            return nullptr;
        }

        ++activeQueryCount;
        int delayMs = 0;
        QString state = QStringLiteral("active");
        if (delayNextStateQueryBriefly) {
            delayNextStateQueryBriefly = false;
            delayMs = 250;
        } else if (delayNextStateQuery) {
            delayNextStateQuery = false;
            delayMs = 10000;
        } else if (activeQueryCount == 1 && delayFirstStateQuery) {
            delayMs = 150;
            state = QStringLiteral("inactive");
        }

        return makeQueryOperation(state, delayMs);
    }

    PlatformOperation *queryBootEnabled() override
    {
        ++bootQueryCount;
        int delayMs = 0;
        QString state = QStringLiteral("disabled");
        if (bootQueryCount == 1 && delayFirstBootQuery) {
            delayMs = 150;
            state = QStringLiteral("enabled");
        }

        return makeQueryOperation(state, delayMs);
    }

    PlatformOperation *startEngine() override
    {
        return startAction(QStringLiteral("start"));
    }

    PlatformOperation *stopEngine() override
    {
        return startAction(QStringLiteral("stop"));
    }

    PlatformOperation *setBootEnabled(bool enabled) override
    {
        return startAction(enabled ? QStringLiteral("enable") : QStringLiteral("disable"));
    }

    void finishHeldAction()
    {
        if (lastPrivilegedOperation && !lastPrivilegedOperation->isFinished()) {
            lastPrivilegedOperation->finish({
                OperationStatus::Succeeded, 0, QStringLiteral("fake.completed")
            });
        }
    }

private:
    PlatformOperation *makeQueryOperation(const QString &output, int delayMs)
    {
        auto *operation = new PlatformOperation(this);
        QTimer::singleShot(delayMs, operation, [operation, output] {
            operation->reportStandardOutput(output.toUtf8());
            operation->finish({
                OperationStatus::Succeeded, 0, QStringLiteral("fake.query_completed")
            });
        });
        return operation;
    }

    PlatformOperation *startAction(const QString &action)
    {
        actions.append(action);
        if (failNextActionToStart) {
            failNextActionToStart = false;
            return nullptr;
        }

        auto *operation = new PlatformOperation(this);
        ++activePrivilegedProcesses;
        maxConcurrentPrivilegedProcesses = qMax(
            maxConcurrentPrivilegedProcesses, activePrivilegedProcesses);
        lastPrivilegedOperation = operation;
        connect(operation, &PlatformOperation::finished, this, [this] {
            --activePrivilegedProcesses;
        });

        if (holdPrivilegedProcessOpen)
            return operation;

        OperationResult result {
            OperationStatus::Succeeded, 0, QStringLiteral("fake.completed")
        };
        if (nextActionExitCode >= 0) {
            const int exitCode = nextActionExitCode;
            nextActionExitCode = -1;
            result = {
                (exitCode == 126 || exitCode == 127)
                    ? OperationStatus::AuthorizationDenied : OperationStatus::Failed,
                exitCode,
                QStringLiteral("fake.action_failed")
            };
        } else if (failNextActionToExecute) {
            failNextActionToExecute = false;
            result = {
                OperationStatus::Failed, 1, QStringLiteral("fake.action_failed")
            };
        }

        int delayMs = 0;
        if (delayNextAction) {
            delayNextAction = false;
            delayMs = 400;
        }

        QTimer::singleShot(delayMs, operation, [operation, result] {
            operation->finish(result);
        });
        return operation;
    }
};

class TestSystemdBackend final : public SystemdBackend
{
    Q_OBJECT

public:
    TestSystemdBackend()
        : TestSystemdBackend(new FakeServiceControlOperations)
    {
    }

    bool shortTimeouts = false;
    FakeServiceControlOperations *fakeOperations = nullptr;

protected:
    int processTimeoutMs(bool privileged) const override
    {
        return shortTimeouts ? 100 : SystemdBackend::processTimeoutMs(privileged);
    }

private:
    explicit TestSystemdBackend(FakeServiceControlOperations *operations)
        : SystemdBackend(operations)
        , fakeOperations(operations)
    {
    }
};

class SystemdBackendTest final : public QObject
{
    Q_OBJECT

private slots:
    void serializesUnitActionsInFifoOrder();
    void continuesAfterFailedToStart();
    void continuesAfterNonzeroExit();
    void correlatedVerificationDoesNotPublishGlobalState();
    void correlatedLifecycleActionsEmitOnlyRequestCompletion();
    void correlatedBootActionEmitsRequestCompletion();
    void lifecycleActionsEmitTypedOperationResults();
    void failedActionReportsTypedFailure();
    void pkexecAuthorizationExitCodesReportDenial();
    void reportsSupportedCapabilitiesAndAuthorizationRequirements();
    void ordinaryVerificationPublishesGlobalState();
    void newerVerificationSupersedesOlderVerification();
    void refreshDoesNotInvalidateLifecycleVerification();
    void refreshDoesNotLaunchOverlappingStateQuery();
    void refreshDoesNotLaunchOverlappingBootQuery();
    void refreshCoalescesWhileQueriesAreRunning();
    void stateQueryTimeoutProducesUnknown();
    void continuesAfterTimedOutAction();
    void timedOutActionReportsTypedTimeout();
    void verificationDuringActionIsInvalidatedWhenActionFinishes();
    void correlatedVerificationInvalidatesOlderRefresh();
    void destroyingBackendDestroysActiveOperations();
    void releasesQueueWhenStateQueryCannotStart();
};

void SystemdBackendTest::serializesUnitActionsInFifoOrder()
{
    TestSystemdBackend backend;
    QSignalSpy finished(&backend, &ZapretBackend::unitActionFinished);

    backend.stop();
    backend.start();
    backend.setBootEnabled(false);
    backend.setBootEnabled(true);

    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 4, 3000);
    QCOMPARE(backend.fakeOperations->actions,
             QStringList({QStringLiteral("stop"), QStringLiteral("start"),
                          QStringLiteral("disable"), QStringLiteral("enable")}));
    QCOMPARE(backend.fakeOperations->maxConcurrentPrivilegedProcesses, 1);
}

void SystemdBackendTest::continuesAfterFailedToStart()
{
    TestSystemdBackend backend;
    backend.fakeOperations->failNextActionToStart = true;
    QSignalSpy finished(&backend, &ZapretBackend::unitActionFinished);

    backend.stop();
    backend.start();

    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 2, 3000);
    QCOMPARE(backend.fakeOperations->actions,
             QStringList({QStringLiteral("stop"), QStringLiteral("start")}));
    QVERIFY(!finished.at(0).at(2).toBool());
    QVERIFY(finished.at(1).at(2).toBool());
}

void SystemdBackendTest::continuesAfterNonzeroExit()
{
    TestSystemdBackend backend;
    backend.fakeOperations->failNextActionToExecute = true;
    QSignalSpy finished(&backend, &ZapretBackend::unitActionFinished);

    backend.stop();
    backend.start();

    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 2, 3000);
    QVERIFY(!finished.at(0).at(2).toBool());
    QVERIFY(finished.at(1).at(2).toBool());
    QCOMPARE(backend.fakeOperations->actions.size(), 2);
}

void SystemdBackendTest::correlatedVerificationDoesNotPublishGlobalState()
{
    TestSystemdBackend backend;
    QSignalSpy globalState(&backend, &ZapretBackend::stateChanged);
    QSignalSpy verified(&backend, &ZapretBackend::stateVerifiedForRequest);

    backend.verifyState(quint64(42));

    QTRY_COMPARE_WITH_TIMEOUT(verified.count(), 1, 3000);
    QCOMPARE(verified.at(0).at(0).toULongLong(), quint64(42));
    QCOMPARE(globalState.count(), 0);
}

void SystemdBackendTest::correlatedLifecycleActionsEmitOnlyRequestCompletion()
{
    TestSystemdBackend backend;
    QSignalSpy genericFinished(&backend, &ZapretBackend::unitActionFinished);
    QSignalSpy requestFinished(&backend, &ZapretBackend::unitActionFinishedForRequest);

    backend.start(quint64(101));
    backend.stop(quint64(202));

    QTRY_COMPARE_WITH_TIMEOUT(requestFinished.count(), 2, 3000);
    QCOMPARE(genericFinished.count(), 0);
    QCOMPARE(requestFinished.at(0).at(0).toULongLong(), quint64(101));
    QCOMPARE(requestFinished.at(0).at(1).toString(), QStringLiteral("start"));
    QCOMPARE(requestFinished.at(1).at(0).toULongLong(), quint64(202));
    QCOMPARE(requestFinished.at(1).at(1).toString(), QStringLiteral("stop"));
}

void SystemdBackendTest::correlatedBootActionEmitsRequestCompletion()
{
    TestSystemdBackend backend;
    QSignalSpy genericFinished(&backend, &ZapretBackend::unitActionFinished);
    QSignalSpy requestFinished(&backend, &ZapretBackend::unitActionFinishedForRequest);

    backend.setBootEnabled(true, quint64(303));

    QTRY_COMPARE_WITH_TIMEOUT(requestFinished.count(), 1, 3000);
    QCOMPARE(genericFinished.count(), 0);
    QCOMPARE(requestFinished.at(0).at(0).toULongLong(), quint64(303));
    QCOMPARE(requestFinished.at(0).at(1).toString(), QStringLiteral("enable"));
}

void SystemdBackendTest::lifecycleActionsEmitTypedOperationResults()
{
    TestSystemdBackend backend;
    QSignalSpy results(&backend, &ZapretBackend::operationFinished);

    backend.start(quint64(404));

    QTRY_COMPARE_WITH_TIMEOUT(results.count(), 1, 3000);
    const auto result = qvariant_cast<Zapret::Backend::OperationResult>(results.at(0).at(0));
    QCOMPARE(result.requestId, quint64(404));
    QCOMPARE(result.operation, Zapret::Backend::Operation::Start);
    QCOMPARE(result.status, Zapret::Backend::OperationStatus::Succeeded);
    QCOMPARE(result.state, Zapret::Backend::EngineState::Active);
    QVERIFY(result.succeeded());
}

void SystemdBackendTest::failedActionReportsTypedFailure()
{
    TestSystemdBackend backend;
    backend.fakeOperations->failNextActionToExecute = true;
    QSignalSpy results(&backend, &ZapretBackend::operationFinished);

    backend.stop(quint64(405));

    QTRY_COMPARE_WITH_TIMEOUT(results.count(), 1, 3000);
    const auto result = qvariant_cast<Zapret::Backend::OperationResult>(results.at(0).at(0));
    QCOMPARE(result.requestId, quint64(405));
    QCOMPARE(result.operation, Zapret::Backend::Operation::Stop);
    QCOMPARE(result.status, Zapret::Backend::OperationStatus::Failed);
    QVERIFY(!result.succeeded());
    QVERIFY(!result.message.isNull());
}

void SystemdBackendTest::pkexecAuthorizationExitCodesReportDenial()
{
    for (const int exitCode : {126, 127}) {
        TestSystemdBackend backend;
        backend.fakeOperations->nextActionExitCode = exitCode;
        QSignalSpy results(&backend, &ZapretBackend::operationFinished);

        backend.stop(quint64(exitCode));

        QTRY_COMPARE_WITH_TIMEOUT(results.count(), 1, 3000);
        const auto result =
            qvariant_cast<Zapret::Backend::OperationResult>(results.at(0).at(0));
        QCOMPARE(result.requestId, quint64(exitCode));
        QCOMPARE(result.operation, Zapret::Backend::Operation::Stop);
        QCOMPARE(result.status, Zapret::Backend::OperationStatus::AuthorizationDenied);
        QVERIFY(!result.succeeded());
        QVERIFY(result.message.contains(QStringLiteral("Авторизация")));
    }
}

void SystemdBackendTest::reportsSupportedCapabilitiesAndAuthorizationRequirements()
{
    using namespace Zapret::Backend;

    const SystemdBackend backend(new FakeServiceControlOperations);
    const auto capabilities = backend.capabilities();
    QCOMPARE(capabilities.size(), 6);

    const auto findCapability = [&capabilities](Capability capability) {
        const auto it = std::find_if(capabilities.cbegin(), capabilities.cend(),
                                     [capability](const CapabilityInfo &info) {
            return info.capability == capability;
        });
        return it == capabilities.cend() ? CapabilityInfo{} : *it;
    };

    const CapabilityInfo query = findCapability(Capability::QueryState);
    QVERIFY(query.isAvailable());
    QVERIFY(!query.requiresAuthorization);

    const CapabilityInfo start = findCapability(Capability::Start);
    QVERIFY(start.isAvailable());
    QVERIFY(start.requiresAuthorization);

    const CapabilityInfo stop = findCapability(Capability::Stop);
    QVERIFY(stop.isAvailable());
    QVERIFY(stop.requiresAuthorization);

    const CapabilityInfo boot = findCapability(Capability::BootEnablement);
    QVERIFY(boot.isAvailable());
    QVERIFY(boot.requiresAuthorization);

    QCOMPARE(findCapability(Capability::Configuration).availability,
             CapabilityAvailability::Unsupported);
    QCOMPARE(findCapability(Capability::Diagnostics).availability,
             CapabilityAvailability::Unsupported);
}

void SystemdBackendTest::ordinaryVerificationPublishesGlobalState()
{
    TestSystemdBackend backend;
    QSignalSpy globalState(&backend, &ZapretBackend::stateChanged);
    QSignalSpy verified(&backend, &ZapretBackend::stateVerified);

    backend.verifyState();

    QTRY_COMPARE_WITH_TIMEOUT(verified.count(), 1, 3000);
    QCOMPARE(globalState.count(), 1);
    QCOMPARE(globalState.at(0).at(0).value<ZapretBackend::State>(), ZapretBackend::State::Active);
}

void SystemdBackendTest::newerVerificationSupersedesOlderVerification()
{
    TestSystemdBackend backend;
    backend.fakeOperations->delayFirstStateQuery = true;
    QSignalSpy globalState(&backend, &ZapretBackend::stateChanged);
    QSignalSpy verified(&backend, &ZapretBackend::stateVerified);

    backend.verifyState();
    backend.verifyState();

    QTRY_COMPARE_WITH_TIMEOUT(verified.count(), 1, 3000);
    QCOMPARE(verified.at(0).at(0).value<ZapretBackend::State>(), ZapretBackend::State::Active);
    QCOMPARE(globalState.count(), 1);
    QCOMPARE(globalState.at(0).at(0).value<ZapretBackend::State>(), ZapretBackend::State::Active);
    QTest::qWait(200);
    QCOMPARE(verified.count(), 1);
    QCOMPARE(globalState.count(), 1);
}

void SystemdBackendTest::refreshDoesNotInvalidateLifecycleVerification()
{
    TestSystemdBackend backend;
    backend.fakeOperations->delayFirstStateQuery = true;
    QSignalSpy globalState(&backend, &ZapretBackend::stateChanged);
    QSignalSpy verified(&backend, &ZapretBackend::stateVerified);

    backend.verifyState();
    backend.refresh();

    QTRY_COMPARE_WITH_TIMEOUT(verified.count(), 1, 3000);
    QCOMPARE(verified.at(0).at(0).value<ZapretBackend::State>(), ZapretBackend::State::Inactive);
    QTRY_COMPARE_WITH_TIMEOUT(globalState.count(), 1, 3000);
    QCOMPARE(globalState.at(0).at(0).value<ZapretBackend::State>(), ZapretBackend::State::Active);
}

void SystemdBackendTest::refreshDoesNotLaunchOverlappingStateQuery()
{
    TestSystemdBackend backend;
    backend.fakeOperations->delayFirstStateQuery = true;
    QSignalSpy globalState(&backend, &ZapretBackend::stateChanged);

    backend.refresh();
    backend.refresh();

    QTRY_COMPARE_WITH_TIMEOUT(globalState.count(), 1, 3000);
    QCOMPARE(globalState.at(0).at(0).value<ZapretBackend::State>(), ZapretBackend::State::Inactive);
    QCOMPARE(backend.fakeOperations->activeQueryCount, 1);
    QTest::qWait(200);
    QCOMPARE(globalState.count(), 1);
}

void SystemdBackendTest::refreshDoesNotLaunchOverlappingBootQuery()
{
    TestSystemdBackend backend;
    backend.fakeOperations->delayFirstBootQuery = true;
    QSignalSpy bootState(&backend, &ZapretBackend::bootEnabledChanged);

    backend.refresh();
    backend.refresh();

    QTRY_COMPARE_WITH_TIMEOUT(bootState.count(), 1, 3000);
    QCOMPARE(bootState.at(0).at(0).toBool(), true);
    QCOMPARE(backend.fakeOperations->bootQueryCount, 1);
    QTest::qWait(200);
    QCOMPARE(bootState.count(), 1);
}

void SystemdBackendTest::refreshCoalescesWhileQueriesAreRunning()
{
    TestSystemdBackend backend;
    backend.fakeOperations->delayFirstStateQuery = true;
    backend.fakeOperations->delayFirstBootQuery = true;
    QSignalSpy globalState(&backend, &ZapretBackend::stateChanged);
    QSignalSpy bootState(&backend, &ZapretBackend::bootEnabledChanged);

    backend.refresh();
    backend.refresh();
    backend.refresh();

    QTRY_COMPARE_WITH_TIMEOUT(globalState.count(), 1, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(bootState.count(), 1, 3000);
    QCOMPARE(backend.fakeOperations->activeQueryCount, 1);
    QCOMPARE(backend.fakeOperations->bootQueryCount, 1);
    QTest::qWait(200);
    QCOMPARE(backend.fakeOperations->activeQueryCount, 1);
    QCOMPARE(backend.fakeOperations->bootQueryCount, 1);
}

void SystemdBackendTest::stateQueryTimeoutProducesUnknown()
{
    TestSystemdBackend backend;
    backend.shortTimeouts = true;
    backend.fakeOperations->delayNextStateQuery = true;
    QSignalSpy globalState(&backend, &ZapretBackend::stateChanged);
    QSignalSpy errors(&backend, &ZapretBackend::errorOccurred);

    backend.refresh();

    QTRY_COMPARE_WITH_TIMEOUT(globalState.count(), 1, 3000);
    QCOMPARE(globalState.at(0).at(0).value<ZapretBackend::State>(), ZapretBackend::State::Unknown);
    QCOMPARE(errors.count(), 1);
}

void SystemdBackendTest::continuesAfterTimedOutAction()
{
    TestSystemdBackend backend;
    backend.shortTimeouts = true;
    backend.fakeOperations->delayNextAction = true;
    QSignalSpy finished(&backend, &ZapretBackend::unitActionFinished);
    QSignalSpy errors(&backend, &ZapretBackend::errorOccurred);

    backend.stop();
    backend.start();

    QTRY_COMPARE_WITH_TIMEOUT(backend.fakeOperations->actions.size(), 1, 1000);
    QTest::qWait(150);
    QCOMPARE(backend.fakeOperations->actions.size(), 1);
    QCOMPARE(finished.count(), 0);
    QCOMPARE(errors.count(), 1);

    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 2, 3000);
    QVERIFY(!finished.at(0).at(2).toBool());
    QVERIFY(finished.at(1).at(2).toBool());
    QCOMPARE(backend.fakeOperations->maxConcurrentPrivilegedProcesses, 1);
}

void SystemdBackendTest::timedOutActionReportsTypedTimeout()
{
    TestSystemdBackend backend;
    backend.shortTimeouts = true;
    backend.fakeOperations->delayNextAction = true;
    QSignalSpy results(&backend, &ZapretBackend::operationFinished);

    backend.stop(quint64(406));

    QTRY_COMPARE_WITH_TIMEOUT(results.count(), 1, 3000);
    const auto result = qvariant_cast<Zapret::Backend::OperationResult>(results.at(0).at(0));
    QCOMPARE(result.requestId, quint64(406));
    QCOMPARE(result.operation, Zapret::Backend::Operation::Stop);
    QCOMPARE(result.status, Zapret::Backend::OperationStatus::TimedOut);
    QVERIFY(!result.succeeded());
}

void SystemdBackendTest::verificationDuringActionIsInvalidatedWhenActionFinishes()
{
    TestSystemdBackend backend;
    backend.fakeOperations->holdPrivilegedProcessOpen = true;
    QSignalSpy verified(&backend, &ZapretBackend::stateVerified);
    QSignalSpy finished(&backend, &ZapretBackend::unitActionFinished);

    backend.start();
    QTRY_VERIFY_WITH_TIMEOUT(backend.fakeOperations->lastPrivilegedOperation, 3000);

    backend.fakeOperations->delayNextStateQueryBriefly = true;
    backend.verifyState();
    backend.fakeOperations->finishHeldAction();

    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 3000);
    QTest::qWait(400);
    QCOMPARE(verified.count(), 0);
}

void SystemdBackendTest::correlatedVerificationInvalidatesOlderRefresh()
{
    TestSystemdBackend backend;
    backend.fakeOperations->delayFirstStateQuery = true;
    QSignalSpy globalState(&backend, &ZapretBackend::stateChanged);
    QSignalSpy verified(&backend, &ZapretBackend::stateVerifiedForRequest);

    backend.refresh();
    backend.verifyState(quint64(73));

    QTRY_COMPARE_WITH_TIMEOUT(verified.count(), 1, 3000);
    QCOMPARE(globalState.count(), 0);
    QTest::qWait(200);
    QCOMPARE(globalState.count(), 0);
}

void SystemdBackendTest::destroyingBackendDestroysActiveOperations()
{
    auto *backend = new TestSystemdBackend;
    backend->fakeOperations->holdPrivilegedProcessOpen = true;
    backend->stop();
    QTRY_VERIFY_WITH_TIMEOUT(backend->fakeOperations->lastPrivilegedOperation, 3000);

    QPointer<PlatformOperation> operation = backend->fakeOperations->lastPrivilegedOperation;
    delete backend;
    QVERIFY(operation.isNull());
}

void SystemdBackendTest::releasesQueueWhenStateQueryCannotStart()
{
    TestSystemdBackend backend;
    backend.fakeOperations->failNextStateQueryToStart = true;
    QSignalSpy finished(&backend, &ZapretBackend::unitActionFinished);

    backend.stop();
    backend.start();

    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 2, 3000);
    QCOMPARE(backend.fakeOperations->actions.size(), 2);
}

QTEST_GUILESS_MAIN(SystemdBackendTest)

#include "test_systemdbackend.moc"
