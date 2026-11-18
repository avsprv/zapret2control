#include <QtTest>

#include <QTimer>

#include "logscontroller.h"

using namespace Zapret::Platform;

class FakeSystemLogOperations final : public QObject, public SystemLogOperations
{
    Q_OBJECT

public:
    QByteArray contents = QByteArray("2026-11-18T13:00:00Z test log entry\n");
    OperationResult result {
        OperationStatus::Succeeded, 0, QStringLiteral("helper.completed")
    };
    LogLineCount lastLineCount = LogLineCount::TwoHundred;
    int calls = 0;
    bool emitUntrustedDiagnostics = true;
    bool returnNull = false;

    PlatformOperation *readRecentLogs(LogLineCount lineCount) override
    {
        ++calls;
        lastLineCount = lineCount;
        if (returnNull)
            return nullptr;

        auto *operation = new PlatformOperation(this);
        const QByteArray output = contents;
        const OperationResult operationResult = result;
        const bool emitDiagnostics = emitUntrustedDiagnostics;
        QTimer::singleShot(0, operation,
                           [operation, output, operationResult, emitDiagnostics] {
            if (operationResult.succeeded())
                operation->reportStandardOutput(output);
            if (emitDiagnostics)
                operation->reportStandardError(QByteArray("UNTRUSTED_HELPER_ERROR"));
            operation->finish(operationResult);
        });
        return operation;
    }
};

class TestLogsController final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void rejectsUnsupportedLineCount();
    void readsServiceLogsThroughTypedOperation();
    void doesNotExposeHelperDiagnostics();
    void rejectsInvalidUtf8();
    void rejectsOversizedOutput();
    void reportsUnavailableProvider();
};

void TestLogsController::rejectsUnsupportedLineCount()
{
    FakeSystemLogOperations operations;
    LogsController controller(&operations);
    controller.readLogs(25);

    QVERIFY(!controller.busy());
    QCOMPARE(controller.lineCount(), 200);
    QCOMPARE(controller.status(), QStringLiteral("Недопустимое количество строк журнала."));
    QCOMPARE(operations.calls, 0);
}

void TestLogsController::readsServiceLogsThroughTypedOperation()
{
    FakeSystemLogOperations operations;
    LogsController controller(&operations);
    controller.readLogs(100);

    QTRY_VERIFY_WITH_TIMEOUT(controller.logText().contains(QStringLiteral("test log entry")), 1000);
    QCOMPARE(controller.lineCount(), 100);
    QCOMPARE(operations.lastLineCount, LogLineCount::OneHundred);
    QVERIFY(!controller.busy());
    QVERIFY(controller.status().contains(QStringLiteral("получено")));
}

void TestLogsController::doesNotExposeHelperDiagnostics()
{
    FakeSystemLogOperations operations;
    operations.result = {
        OperationStatus::Failed, 13, QStringLiteral("helper.exit_failed")
    };
    LogsController controller(&operations);
    controller.readLogs(50);

    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);
    QCOMPARE(controller.status(), QStringLiteral("Не удалось прочитать журнал (код 13)."));
    QVERIFY(!controller.status().contains(QStringLiteral("UNTRUSTED_HELPER")));
    QVERIFY(controller.logText().isEmpty());
}

void TestLogsController::rejectsInvalidUtf8()
{
    FakeSystemLogOperations operations;
    operations.contents = QByteArray("\xff\xfe", 2);
    LogsController controller(&operations);
    controller.readLogs(200);

    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);
    QVERIFY(controller.logText().isEmpty());
    QVERIFY(controller.status().contains(QStringLiteral("некорректный UTF-8")));
}

void TestLogsController::rejectsOversizedOutput()
{
    FakeSystemLogOperations operations;
    operations.contents = QByteArray(1024 * 1024 + 1, 'x');
    LogsController controller(&operations);
    controller.readLogs(500);

    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);
    QVERIFY(controller.logText().isEmpty());
    QVERIFY(controller.status().contains(QStringLiteral("превышает ограничение 1 MiB")));
}

void TestLogsController::reportsUnavailableProvider()
{
    FakeSystemLogOperations operations;
    operations.returnNull = true;
    LogsController controller(&operations);
    controller.readLogs(100);

    QVERIFY(!controller.busy());
    QCOMPARE(controller.status(), QStringLiteral("Не удалось прочитать журнал (код -1)."));
}

QTEST_GUILESS_MAIN(TestLogsController)

#include "test_logscontroller.moc"
