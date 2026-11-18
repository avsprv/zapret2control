#include <QtTest>

#include "platform/linuxsystemlogoperations.h"

#include <QFile>
#include <QTemporaryDir>

using namespace Zapret::Platform;

class TestLinuxSystemLogOperations final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void readsOnlyTheRequestedFixedLogLimit();
    void rejectsInvalidLineCount();
    void rejectsOversizedLogOutput();
    void rejectsOversizedDiagnostics();
    void mapsAuthorizationCancellation();
    void enforcesOperationTimeout();

private:
    QTemporaryDir m_directory;
    QString m_helperPath;
};

void TestLinuxSystemLogOperations::initTestCase()
{
    QVERIFY(m_directory.isValid());
    m_helperPath = m_directory.filePath(QStringLiteral("fake-helper"));

    QFile helper(m_helperPath);
    QVERIFY(helper.open(QIODevice::WriteOnly | QIODevice::Text));
    const QByteArray script = R"(#!/bin/sh
set -eu
case "${QZAPRET_FAKE_MODE:-}" in
    deny) exit 126 ;;
    timeout) sleep 2; exit 0 ;;
    oversized) head -c 1048577 /dev/zero; exit 0 ;;
    stderr-oversized) head -c 65537 /dev/zero >&2; exit 1 ;;
esac
[ "$1" = logs-read ] || exit 64
[ "$2" = 100 ] || exit 65
printf '2026-11-18T13:00:00Z typed log entry\n'
echo "UNTRUSTED_HELPER_ERROR" >&2
)";
    QCOMPARE(helper.write(script), script.size());
    helper.close();
    QVERIFY(helper.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    qputenv("QZAPRET_HELPER", m_helperPath.toUtf8());
    qputenv("QZAPRET_NO_PKEXEC", "1");
}

void TestLinuxSystemLogOperations::readsOnlyTheRequestedFixedLogLimit()
{
    qunsetenv("QZAPRET_FAKE_MODE");
    LinuxSystemLogOperations operations;
    PlatformOperation *operation = operations.readRecentLogs(LogLineCount::OneHundred);
    QSignalSpy outputSpy(operation, &PlatformOperation::standardOutputReceived);
    QSignalSpy errorSpy(operation, &PlatformOperation::standardErrorReceived);
    QSignalSpy finishedSpy(operation, &PlatformOperation::finished);

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 3000);
    QCOMPARE(finishedSpy.constFirst().constFirst().value<OperationResult>().status,
             OperationStatus::Succeeded);
    QVERIFY(!outputSpy.isEmpty());
    QCOMPARE(outputSpy.constFirst().constFirst().toByteArray(),
             QByteArray("2026-11-18T13:00:00Z typed log entry\n"));
    QCOMPARE(errorSpy.size(), 0);
}

void TestLinuxSystemLogOperations::rejectsInvalidLineCount()
{
    LinuxSystemLogOperations operations;
    PlatformOperation *operation = operations.readRecentLogs(
        static_cast<LogLineCount>(75));
    QSignalSpy finishedSpy(operation, &PlatformOperation::finished);

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 1000);
    QCOMPARE(finishedSpy.constFirst().constFirst().value<OperationResult>().status,
             OperationStatus::ProtocolError);
}

void TestLinuxSystemLogOperations::rejectsOversizedLogOutput()
{
    qputenv("QZAPRET_FAKE_MODE", "oversized");
    LinuxSystemLogOperations operations;
    PlatformOperation *operation = operations.readRecentLogs(LogLineCount::OneHundred);
    QSignalSpy finishedSpy(operation, &PlatformOperation::finished);

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 3000);
    QCOMPARE(finishedSpy.constFirst().constFirst().value<OperationResult>().status,
             OperationStatus::ProtocolError);
    qunsetenv("QZAPRET_FAKE_MODE");
}

void TestLinuxSystemLogOperations::rejectsOversizedDiagnostics()
{
    qputenv("QZAPRET_FAKE_MODE", "stderr-oversized");
    LinuxSystemLogOperations operations;
    PlatformOperation *operation = operations.readRecentLogs(LogLineCount::OneHundred);
    QSignalSpy errorSpy(operation, &PlatformOperation::standardErrorReceived);
    QSignalSpy finishedSpy(operation, &PlatformOperation::finished);

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 3000);
    QCOMPARE(finishedSpy.constFirst().constFirst().value<OperationResult>().status,
             OperationStatus::ProtocolError);
    QCOMPARE(errorSpy.size(), 0);
    qunsetenv("QZAPRET_FAKE_MODE");
}

void TestLinuxSystemLogOperations::mapsAuthorizationCancellation()
{
    qputenv("QZAPRET_FAKE_MODE", "deny");
    LinuxSystemLogOperations operations;
    PlatformOperation *operation = operations.readRecentLogs(LogLineCount::OneHundred);
    QSignalSpy finishedSpy(operation, &PlatformOperation::finished);

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 3000);
    const auto result = finishedSpy.constFirst().constFirst().value<OperationResult>();
    QCOMPARE(result.status, OperationStatus::AuthorizationDenied);
    QCOMPARE(result.nativeCode, 126);
    qunsetenv("QZAPRET_FAKE_MODE");
}

void TestLinuxSystemLogOperations::enforcesOperationTimeout()
{
    qputenv("QZAPRET_FAKE_MODE", "timeout");
    LinuxSystemLogOperations operations(100);
    PlatformOperation *operation = operations.readRecentLogs(LogLineCount::OneHundred);
    QSignalSpy finishedSpy(operation, &PlatformOperation::finished);

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 3000);
    QCOMPARE(finishedSpy.constFirst().constFirst().value<OperationResult>().status,
             OperationStatus::TimedOut);
    qunsetenv("QZAPRET_FAKE_MODE");
}

QTEST_GUILESS_MAIN(TestLinuxSystemLogOperations)

#include "test_linuxsystemlogoperations.moc"
