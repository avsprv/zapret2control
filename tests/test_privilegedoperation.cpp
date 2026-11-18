#include <QtTest>

#include "platform/privilegedoperation.h"
#include "platform/privilegedoperations.h"

using namespace Zapret::Platform;

class TestPlatformOperation final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void streamsOutputBeforeCompletion();
    void ignoresOutputAfterCompletion();
    void completesExactlyOnce();
    void validatesSha256Digests();
    void validatesUpstreamVersionTags();
    void validatesBlockcheckOptions();
};

void TestPlatformOperation::initTestCase()
{
    qRegisterMetaType<OperationStatus>();
    qRegisterMetaType<OperationResult>();
}

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

void TestPlatformOperation::validatesSha256Digests()
{
    const QByteArray valid(64, 'a');
    const auto digest = Sha256Digest::fromHex(valid);
    QVERIFY(digest.has_value());
    QCOMPARE(digest->hex(), valid);

    QVERIFY(!Sha256Digest::fromHex(QByteArray(63, 'a')).has_value());
    QVERIFY(!Sha256Digest::fromHex(QByteArray(64, 'g')).has_value());
    QVERIFY(!Sha256Digest::fromHex(QByteArray(64, 'A')).has_value());
}

void TestPlatformOperation::validatesUpstreamVersionTags()
{
    const auto version = ValidatedUpstreamVersion::fromTag(QStringLiteral("v1.2.3"));
    QVERIFY(version.has_value());
    QCOMPARE(version->tag(), QStringLiteral("v1.2.3"));

    QVERIFY(!ValidatedUpstreamVersion::fromTag(QString()).has_value());
    QVERIFY(!ValidatedUpstreamVersion::fromTag(QStringLiteral("1.2.3")).has_value());
    QVERIFY(!ValidatedUpstreamVersion::fromTag(QStringLiteral("v1..2")).has_value());
    QVERIFY(!ValidatedUpstreamVersion::fromTag(QStringLiteral("v1.2.")).has_value());
    QVERIFY(!ValidatedUpstreamVersion::fromTag(QStringLiteral("v1.2-rc1")).has_value());
    QVERIFY(!ValidatedUpstreamVersion::fromTag(QStringLiteral("v123456789012345678901234")).has_value());
}

void TestPlatformOperation::validatesBlockcheckOptions()
{
    BlockcheckOptions options;
    QVERIFY(options.isValid());

    options.domains = {};
    QVERIFY(!options.isValid());

    options.domains = {QStringLiteral("example.org"), QStringLiteral("example.net")};
    options.repeats = 10;
    QVERIFY(options.isValid());

    options.repeats = 11;
    QVERIFY(!options.isValid());

    options.repeats = 1;
    options.domains = {QStringLiteral("bad..example")};
    QVERIFY(!options.isValid());

    options.domains = {QStringLiteral("-bad.example")};
    QVERIFY(!options.isValid());

    options.domains = {QStringLiteral("example.org.")};
    QVERIFY(options.isValid());

    options.domains = {QStringLiteral("пример.рф")};
    QVERIFY(!options.isValid());
}

QTEST_GUILESS_MAIN(TestPlatformOperation)

#include "test_privilegedoperation.moc"
