#include <QtTest>

#include "platform/privilegedoperation.h"

using namespace Zapret::Platform;

class TestPlatformOperation final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void streamsOutputBeforeCompletion();
    void ignoresOutputAfterCompletion();
    void completesExactlyOnce();
};

void TestPlatformOperation::streamsOutputBeforeCompletion()
{
    PlatformOperation operation;
    QSignalSpy standardOutputSpy(&operation, &PlatformOperation::standardOutputReceived);
    QSignalSpy standardErrorSpy(&operation, &PlatformOperation::standardErrorReceived);
    QSignalSpy finishedSpy(&operation, &PlatformOperation::finished);

    operation.reportStandardOutput(QByteArray("stdout"));
    operation.reportStandardError(QByteArray("stderr"));

    QCOMPARE(standardOutputSpy.size(), 1);
    QCOMPARE(standardOutputSpy.constFirst().constFirst().toByteArray(), QByteArray("stdout"));
    QCOMPARE(standardErrorSpy.size(), 1);
    QCOMPARE(standardErrorSpy.constFirst().constFirst().toByteArray(), QByteArray("stderr"));
    QCOMPARE(finishedSpy.size(), 0);

    const OperationResult result{OperationStatus::Succeeded, 0, {}};
    operation.finish(result);

    QCOMPARE(finishedSpy.size(), 1);
    const auto emittedResult =
        qvariant_cast<OperationResult>(finishedSpy.constFirst().constFirst());
    QCOMPARE(emittedResult.status, OperationStatus::Succeeded);
    QVERIFY(operation.isFinished());
}

void TestPlatformOperation::ignoresOutputAfterCompletion()
{
    PlatformOperation operation;
    QSignalSpy standardOutputSpy(&operation, &PlatformOperation::standardOutputReceived);
    QSignalSpy standardErrorSpy(&operation, &PlatformOperation::standardErrorReceived);

    operation.finish({OperationStatus::Failed, 17, QStringLiteral("test.failure")});
    operation.reportStandardOutput(QByteArray("late stdout"));
    operation.reportStandardError(QByteArray("late stderr"));

    QCOMPARE(standardOutputSpy.size(), 0);
    QCOMPARE(standardErrorSpy.size(), 0);
}

void TestPlatformOperation::completesExactlyOnce()
{
    PlatformOperation operation;
    QSignalSpy finishedSpy(&operation, &PlatformOperation::finished);

    operation.finish({OperationStatus::Succeeded, 0, {}});
    operation.finish({OperationStatus::Failed, 1, QStringLiteral("late.failure")});

    QCOMPARE(finishedSpy.size(), 1);
    const auto result = qvariant_cast<OperationResult>(finishedSpy.constFirst().constFirst());
    QCOMPARE(result.status, OperationStatus::Succeeded);
    QCOMPARE(result.nativeCode, 0);
}

QTEST_GUILESS_MAIN(TestPlatformOperation)

#include "test_privilegedoperation.moc"
