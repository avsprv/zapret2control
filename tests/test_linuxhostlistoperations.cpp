#include <QtTest>

#include "platform/linuxhostlistoperations.h"

#include <QFile>
#include <QTemporaryDir>

using namespace Zapret::Platform;

class TestLinuxHostListOperations final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void readsAllowlistedList();
    void mapsMissingListToEmptyResult();
    void writesOnlySourceListsAndPassesBoundedInput();
    void rejectsOversizedInput();
    void rejectsOversizedOutput();
    void mapsAuthorizationCancellation();
    void enforcesOperationTimeout();

private:
    QTemporaryDir m_directory;
    QString m_helperPath;
    QString m_inputPath;
};

void TestLinuxHostListOperations::initTestCase()
{
    QVERIFY(m_directory.isValid());
    m_helperPath = m_directory.filePath(QStringLiteral("fake-helper"));
    m_inputPath = m_directory.filePath(QStringLiteral("stdin.bin"));

    QFile helper(m_helperPath);
    QVERIFY(helper.open(QIODevice::WriteOnly | QIODevice::Text));
    const QByteArray script = R"(#!/bin/sh
set -eu
case "${QZAPRET_FAKE_MODE:-}" in
    deny) exit 126 ;;
    timeout) sleep 2; exit 0 ;;
    oversized) head -c 1048577 /dev/zero; exit 0 ;;
esac
case "$1" in
    lists-read)
        case "${QZAPRET_FAKE_MODE:-}" in missing)
            echo "zapret2control-helper: list file does not exist" >&2
            exit 3
            ;;
        esac
        [ "$2" = zapret-hosts-user.txt ] || exit 64
        printf 'example.org\n'
        ;;
    lists-write)
        [ "$2" = zapret-hosts-user.txt ] || exit 64
        cat > "$QZAPRET_FAKE_INPUT"
        echo "UNTRUSTED_HELPER_OUTPUT"
        echo "UNTRUSTED_HELPER_ERROR" >&2
        ;;
    *) exit 64 ;;
esac
)";
    QCOMPARE(helper.write(script), script.size());
    helper.close();
    QVERIFY(helper.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    qputenv("QZAPRET_HELPER", m_helperPath.toUtf8());
    qputenv("QZAPRET_NO_PKEXEC", "1");
    qputenv("QZAPRET_FAKE_INPUT", m_inputPath.toUtf8());
}

void TestLinuxHostListOperations::readsAllowlistedList()
{
    qunsetenv("QZAPRET_FAKE_MODE");
    LinuxHostListOperations operations;
    PlatformOperation *operation = operations.readList(HostListId::ZapretHostsUser);
    QSignalSpy outputSpy(operation, &PlatformOperation::standardOutputReceived);
    QSignalSpy errorSpy(operation, &PlatformOperation::standardErrorReceived);
    QSignalSpy finishedSpy(operation, &PlatformOperation::finished);

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 3000);
    QCOMPARE(finishedSpy.constFirst().constFirst().value<OperationResult>().status,
             OperationStatus::Succeeded);
    QCOMPARE(outputSpy.size(), 1);
    QCOMPARE(outputSpy.constFirst().constFirst().toByteArray(), QByteArray("example.org\n"));
    QCOMPARE(errorSpy.size(), 0);
}

void TestLinuxHostListOperations::mapsMissingListToEmptyResult()
{
    qputenv("QZAPRET_FAKE_MODE", "missing");
    LinuxHostListOperations operations;
    PlatformOperation *operation = operations.readList(HostListId::ZapretHostsUser);
    QSignalSpy errorSpy(operation, &PlatformOperation::standardErrorReceived);
    QSignalSpy finishedSpy(operation, &PlatformOperation::finished);

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 3000);
    const auto result = finishedSpy.constFirst().constFirst().value<OperationResult>();
    QCOMPARE(result.status, OperationStatus::Succeeded);
    QCOMPARE(result.diagnosticCode, QStringLiteral("hostlist.not_found"));
    QCOMPARE(errorSpy.size(), 0);
    qunsetenv("QZAPRET_FAKE_MODE");
}

void TestLinuxHostListOperations::writesOnlySourceListsAndPassesBoundedInput()
{
    qunsetenv("QZAPRET_FAKE_MODE");
    const QByteArray contents("example.org\n192.0.2.0/24\n");
    LinuxHostListOperations operations;
    PlatformOperation *operation = operations.writeList(
        HostListId::ZapretHostsUser, contents);
    QSignalSpy outputSpy(operation, &PlatformOperation::standardOutputReceived);
    QSignalSpy errorSpy(operation, &PlatformOperation::standardErrorReceived);
    QSignalSpy finishedSpy(operation, &PlatformOperation::finished);

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 3000);
    QCOMPARE(finishedSpy.constFirst().constFirst().value<OperationResult>().status,
             OperationStatus::Succeeded);
    QCOMPARE(outputSpy.size(), 0);
    QCOMPARE(errorSpy.size(), 0);

    QFile input(m_inputPath);
    QVERIFY(input.open(QIODevice::ReadOnly));
    QCOMPARE(input.readAll(), contents);

    PlatformOperation *readOnlyOperation = operations.writeList(
        HostListId::ZapretIpUser, contents);
    QSignalSpy readOnlyFinishedSpy(readOnlyOperation, &PlatformOperation::finished);
    QCOMPARE(readOnlyFinishedSpy.size(), 1);
    QCOMPARE(readOnlyFinishedSpy.constFirst().constFirst().value<OperationResult>().status,
             OperationStatus::ProtocolError);
}

void TestLinuxHostListOperations::rejectsOversizedInput()
{
    LinuxHostListOperations operations;
    PlatformOperation *operation = operations.writeList(
        HostListId::ZapretHostsUser, QByteArray(1024 * 1024 + 1, 'x'));
    QSignalSpy finishedSpy(operation, &PlatformOperation::finished);

    QCOMPARE(finishedSpy.size(), 1);
    QCOMPARE(finishedSpy.constFirst().constFirst().value<OperationResult>().status,
             OperationStatus::ProtocolError);
}

void TestLinuxHostListOperations::rejectsOversizedOutput()
{
    qputenv("QZAPRET_FAKE_MODE", "oversized");
    LinuxHostListOperations operations;
    PlatformOperation *operation = operations.readList(HostListId::ZapretHostsUser);
    QSignalSpy finishedSpy(operation, &PlatformOperation::finished);

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 3000);
    QCOMPARE(finishedSpy.constFirst().constFirst().value<OperationResult>().status,
             OperationStatus::ProtocolError);
    qunsetenv("QZAPRET_FAKE_MODE");
}

void TestLinuxHostListOperations::mapsAuthorizationCancellation()
{
    qputenv("QZAPRET_FAKE_MODE", "deny");
    LinuxHostListOperations operations;
    PlatformOperation *operation = operations.readList(HostListId::ZapretHostsUser);
    QSignalSpy finishedSpy(operation, &PlatformOperation::finished);

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 3000);
    const auto result = finishedSpy.constFirst().constFirst().value<OperationResult>();
    QCOMPARE(result.status, OperationStatus::AuthorizationDenied);
    QCOMPARE(result.nativeCode, 126);
    qunsetenv("QZAPRET_FAKE_MODE");
}

void TestLinuxHostListOperations::enforcesOperationTimeout()
{
    qputenv("QZAPRET_FAKE_MODE", "timeout");
    LinuxHostListOperations operations(100);
    PlatformOperation *operation = operations.readList(HostListId::ZapretHostsUser);
    QSignalSpy finishedSpy(operation, &PlatformOperation::finished);

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 3000);
    QCOMPARE(finishedSpy.constFirst().constFirst().value<OperationResult>().status,
             OperationStatus::TimedOut);
    qunsetenv("QZAPRET_FAKE_MODE");
}

QTEST_GUILESS_MAIN(TestLinuxHostListOperations)

#include "test_linuxhostlistoperations.moc"
