#include <QtTest>

#include "platform/linuxconfigurationoperations.h"

#include <QFile>
#include <QTemporaryDir>

using namespace Zapret::Platform;

class TestLinuxConfigurationOperations final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void readStreamsConfiguration();
    void writePassesExpectedDigestAndBoundedInput();
    void rollbackCompletesSuccessfully();
    void mapsAuthorizationCancellation();
    void enforcesOperationTimeout();

private:
    QTemporaryDir m_directory;
    QString m_helperPath;
    QString m_inputPath;
};

void TestLinuxConfigurationOperations::initTestCase()
{
    QVERIFY(m_directory.isValid());
    m_helperPath = m_directory.filePath(QStringLiteral("fake-helper"));
    m_inputPath = m_directory.filePath(QStringLiteral("stdin.bin"));

    QFile helper(m_helperPath);
    QVERIFY(helper.open(QIODevice::WriteOnly | QIODevice::Text));
    const QByteArray script = R"(#!/bin/sh
set -eu
if [ "${QZAPRET_FAKE_MODE:-}" = "deny" ]; then
    exit 126
fi
if [ "${QZAPRET_FAKE_MODE:-}" = "timeout" ]; then
    sleep 2
    exit 0
fi
case "$1" in
    config-read) printf 'CONFIG=1\n' ;;
    config-write)
        cat > "$QZAPRET_FAKE_INPUT"
        printf '%s' "$2"
        ;;
    config-rollback) printf 'rollback-ok' ;;
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

void TestLinuxConfigurationOperations::readStreamsConfiguration()
{
    qunsetenv("QZAPRET_FAKE_MODE");
    LinuxConfigurationOperations operations;
    PlatformOperation *operation = operations.readConfiguration();
    QSignalSpy outputSpy(operation, &PlatformOperation::standardOutputReceived);
    QSignalSpy finishedSpy(operation, &PlatformOperation::finished);

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 3000);
    QCOMPARE(finishedSpy.constFirst().constFirst().value<OperationResult>().status,
             OperationStatus::Succeeded);
    QCOMPARE(outputSpy.size(), 1);
    QCOMPARE(outputSpy.constFirst().constFirst().toByteArray(), QByteArray("CONFIG=1\n"));
}

void TestLinuxConfigurationOperations::writePassesExpectedDigestAndBoundedInput()
{
    qunsetenv("QZAPRET_FAKE_MODE");
    const QByteArray contents("CONFIG=updated\n");
    const QByteArray digestHex(64, 'a');
    const auto digest = Sha256Digest::fromHex(digestHex);
    QVERIFY(digest.has_value());

    LinuxConfigurationOperations operations;
    PlatformOperation *operation = operations.writeConfiguration(contents, *digest);
    QSignalSpy outputSpy(operation, &PlatformOperation::standardOutputReceived);
    QSignalSpy finishedSpy(operation, &PlatformOperation::finished);

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 3000);
    QCOMPARE(finishedSpy.constFirst().constFirst().value<OperationResult>().status,
             OperationStatus::Succeeded);
    QCOMPARE(outputSpy.constFirst().constFirst().toByteArray(), digestHex);

    QFile input(m_inputPath);
    QVERIFY(input.open(QIODevice::ReadOnly));
    QCOMPARE(input.readAll(), contents);
}

void TestLinuxConfigurationOperations::rollbackCompletesSuccessfully()
{
    qunsetenv("QZAPRET_FAKE_MODE");
    LinuxConfigurationOperations operations;
    PlatformOperation *operation = operations.rollbackConfiguration();
    QSignalSpy outputSpy(operation, &PlatformOperation::standardOutputReceived);
    QSignalSpy finishedSpy(operation, &PlatformOperation::finished);

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 3000);
    QCOMPARE(finishedSpy.constFirst().constFirst().value<OperationResult>().status,
             OperationStatus::Succeeded);
    QCOMPARE(outputSpy.constFirst().constFirst().toByteArray(), QByteArray("rollback-ok"));
}

void TestLinuxConfigurationOperations::mapsAuthorizationCancellation()
{
    qputenv("QZAPRET_FAKE_MODE", "deny");
    LinuxConfigurationOperations operations;
    PlatformOperation *operation = operations.readConfiguration();
    QSignalSpy finishedSpy(operation, &PlatformOperation::finished);

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 3000);
    const auto result = finishedSpy.constFirst().constFirst().value<OperationResult>();
    QCOMPARE(result.status, OperationStatus::AuthorizationDenied);
    QCOMPARE(result.nativeCode, 126);
    qunsetenv("QZAPRET_FAKE_MODE");
}

void TestLinuxConfigurationOperations::enforcesOperationTimeout()
{
    qputenv("QZAPRET_FAKE_MODE", "timeout");
    LinuxConfigurationOperations operations(100);
    PlatformOperation *operation = operations.readConfiguration();
    QSignalSpy finishedSpy(operation, &PlatformOperation::finished);

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 3000);
    QCOMPARE(finishedSpy.constFirst().constFirst().value<OperationResult>().status,
             OperationStatus::TimedOut);
    qunsetenv("QZAPRET_FAKE_MODE");
}

QTEST_GUILESS_MAIN(TestLinuxConfigurationOperations)

#include "test_linuxconfigurationoperations.moc"
