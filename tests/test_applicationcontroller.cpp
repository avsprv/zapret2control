#include <QtTest>
#include <QPointer>

#include "qml/applicationcontroller.h"

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
        if (!deferActions)
            emit unitActionFinishedForRequest(requestId, QStringLiteral("start"),
                                              ZapretBackend::State::Active, true);
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
        if (!deferActions)
            emit unitActionFinishedForRequest(requestId, QStringLiteral("stop"),
                                              ZapretBackend::State::Inactive, true);
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

    emit backend->unitActionFinishedForRequest(
        requestId + 1, QStringLiteral("start"), ZapretBackend::State::Active, true);
    QVERIFY(controller.operationsBusy());
    QVERIFY(!controller.serviceActive());

    emit backend->unitActionFinishedForRequest(
        requestId, QStringLiteral("start"), ZapretBackend::State::Active, true);
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

QTEST_GUILESS_MAIN(ApplicationControllerTest)
#include "test_applicationcontroller.moc"
