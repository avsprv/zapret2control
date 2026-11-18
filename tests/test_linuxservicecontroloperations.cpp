#include <QtTest>

#include "platform/linuxservicecontroloperations.h"

#include <QFile>
#include <QTemporaryDir>

using namespace Zapret::Platform;

class TestLinuxServiceControlOperations final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void queriesFixedServiceState();
    void queriesBootStateWithSystemctlNonzeroExit();
    void startsFixedServiceWithoutExposingHelperOutput();
    void mapsAuthorizationDenial();
    void enforcesActionTimeout();
    void enforcesQueryTimeout();
    void rejectsOversizedQueryOutput();
    void rejectsOversizedQueryDiagnostics();

private:
    bool writeExecutable(const QString &path, const QByteArray &contents);
    QTemporaryDir m_directory;
    QString m_helperPath;
    QString m_originalPath;
};

bool TestLinuxServiceControlOperations::writeExecutable(
    const QString &path, const QByteArray &contents)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    if (file.write(contents) != contents.size())
        return false;
    file.close();
    return file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
}

void TestLinuxServiceControlOperations::initTestCase()
{
    QVERIFY(m_directory.isValid());
    m_helperPath = m_directory.filePath(QStringLiteral("fake-helper"));
    m_originalPath = qEnvironmentVariable("PATH");

    const QString systemctlPath = m_directory.filePath(QStringLiteral("systemctl"));
    const QByteArray systemctlScript = R"(#!/bin/sh
set -eu
case "$1" in
    is-active)
        case "${QZAPRET_SYSTEMCTL_MODE:-}" in
            oversized) head -c 4097 /dev/zero; exit 0 ;;
            stderr-oversized) head -c 65537 /dev/zero >&2; exit 1 ;;
            timeout) sleep 2; exit 0 ;;
        esac
        printf 'active\n'
        ;;
    is-enabled)
        printf 'disabled\n'
        exit 1
        ;;
    *) exit 64 ;;
esac
)";
    QVERIFY(writeExecutable(systemctlPath, systemctlScript));

    const QByteArray helperScript = R"(#!/bin/sh
set -eu
[ "$1" = unit ] || exit 64
case "$2" in
    start)
        echo "UNTRUSTED_HELPER_OUTPUT"
        echo "UNTRUSTED_HELPER_ERROR" >&2
        exit 0
        ;;
    stop) exit 126 ;;
    enable) sleep 2; exit 0 ;;
    disable) exit 1 ;;
    *) exit 64 ;;
esac
)";
    QVERIFY(writeExecutable(m_helperPath, helperScript));

    qputenv("PATH", (m_directory.path().toUtf8() + ':' + m_originalPath.toUtf8()));
    qputenv("QZAPRET_HELPER", m_helperPath.toUtf8());
    qputenv("QZAPRET_NO_PKEXEC", "1");
}

void TestLinuxServiceControlOperations::queriesFixedServiceState()
{
    LinuxServiceControlOperations operations;
    PlatformOperation *operation = operations.queryEngineState();
    QSignalSpy outputSpy(operation, &PlatformOperation::standardOutputReceived);
    QSignalSpy errorSpy(operation, &PlatformOperation::standardErrorReceived);
    QSignalSpy finishedSpy(operation, &PlatformOperation::finished);

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 3000);
    QCOMPARE(finishedSpy.constFirst().constFirst().value<OperationResult>().status,
             OperationStatus::Succeeded);
    QCOMPARE(outputSpy.size(), 1);
    QCOMPARE(outputSpy.constFirst().constFirst().toByteArray(), QByteArray("active\n"));
    QCOMPARE(errorSpy.size(), 0);
}

void TestLinuxServiceControlOperations::queriesBootStateWithSystemctlNonzeroExit()
{
    LinuxServiceControlOperations operations;
    PlatformOperation *operation = operations.queryBootEnabled();
    QSignalSpy outputSpy(operation, &PlatformOperation::standardOutputReceived);
    QSignalSpy finishedSpy(operation, &PlatformOperation::finished);

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 3000);
    QCOMPARE(finishedSpy.constFirst().constFirst().value<OperationResult>().status,
             OperationStatus::Succeeded);
    QCOMPARE(outputSpy.constFirst().constFirst().toByteArray(), QByteArray("disabled\n"));
    QCOMPARE(finishedSpy.constFirst().constFirst().value<OperationResult>().nativeCode, 1);
}

void TestLinuxServiceControlOperations::startsFixedServiceWithoutExposingHelperOutput()
{
    LinuxServiceControlOperations operations;
    PlatformOperation *operation = operations.startEngine();
    QSignalSpy outputSpy(operation, &PlatformOperation::standardOutputReceived);
    QSignalSpy errorSpy(operation, &PlatformOperation::standardErrorReceived);
    QSignalSpy finishedSpy(operation, &PlatformOperation::finished);

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 3000);
    QCOMPARE(finishedSpy.constFirst().constFirst().value<OperationResult>().status,
             OperationStatus::Succeeded);
    QCOMPARE(outputSpy.size(), 0);
    QCOMPARE(errorSpy.size(), 0);
}

void TestLinuxServiceControlOperations::mapsAuthorizationDenial()
{
    LinuxServiceControlOperations operations;
    PlatformOperation *operation = operations.stopEngine();
    QSignalSpy finishedSpy(operation, &PlatformOperation::finished);

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 3000);
    const auto result = finishedSpy.constFirst().constFirst().value<OperationResult>();
    QCOMPARE(result.status, OperationStatus::AuthorizationDenied);
    QCOMPARE(result.nativeCode, 126);
}

void TestLinuxServiceControlOperations::enforcesActionTimeout()
{
    LinuxServiceControlOperations operations(15000, 100);
    PlatformOperation *operation = operations.setBootEnabled(true);
    QSignalSpy finishedSpy(operation, &PlatformOperation::finished);

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 3000);
    QCOMPARE(finishedSpy.constFirst().constFirst().value<OperationResult>().status,
             OperationStatus::TimedOut);
}

void TestLinuxServiceControlOperations::enforcesQueryTimeout()
{
    qputenv("QZAPRET_SYSTEMCTL_MODE", "timeout");
    LinuxServiceControlOperations operations(100, 600000);
    PlatformOperation *operation = operations.queryEngineState();
    QSignalSpy finishedSpy(operation, &PlatformOperation::finished);

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 3000);
    QCOMPARE(finishedSpy.constFirst().constFirst().value<OperationResult>().status,
             OperationStatus::TimedOut);
    qunsetenv("QZAPRET_SYSTEMCTL_MODE");
}

void TestLinuxServiceControlOperations::rejectsOversizedQueryOutput()
{
    qputenv("QZAPRET_SYSTEMCTL_MODE", "oversized");
    LinuxServiceControlOperations operations;
    PlatformOperation *operation = operations.queryEngineState();
    QSignalSpy finishedSpy(operation, &PlatformOperation::finished);

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 3000);
    QCOMPARE(finishedSpy.constFirst().constFirst().value<OperationResult>().status,
             OperationStatus::ProtocolError);
    qunsetenv("QZAPRET_SYSTEMCTL_MODE");
}

void TestLinuxServiceControlOperations::rejectsOversizedQueryDiagnostics()
{
    qputenv("QZAPRET_SYSTEMCTL_MODE", "stderr-oversized");
    LinuxServiceControlOperations operations;
    PlatformOperation *operation = operations.queryEngineState();
    QSignalSpy errorSpy(operation, &PlatformOperation::standardErrorReceived);
    QSignalSpy finishedSpy(operation, &PlatformOperation::finished);

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 3000);
    QCOMPARE(finishedSpy.constFirst().constFirst().value<OperationResult>().status,
             OperationStatus::ProtocolError);
    QCOMPARE(errorSpy.size(), 0);
    qunsetenv("QZAPRET_SYSTEMCTL_MODE");
}

QTEST_GUILESS_MAIN(TestLinuxServiceControlOperations)

#include "test_linuxservicecontroloperations.moc"
