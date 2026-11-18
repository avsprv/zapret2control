#include <QtTest>
#include <QCryptographicHash>
#include <QTimer>
#include "profilecontroller.h"

using namespace Zapret::Platform;

class FakeConfigurationOperations final : public QObject, public ConfigurationOperations
{
    Q_OBJECT
public:
    QByteArray contents =
        QByteArray("NFQWS2_ENABLE=1\nNFQWS2_OPT=\"--filter-tcp=80 --payload=http_req\"\n");
    QByteArray previousContents;
    bool backupCreated = false;
    bool oversizedRead = false;

    PlatformOperation *readConfiguration() override
    {
        auto *operation = new PlatformOperation(this);
        const QByteArray snapshot = contents;
        const bool oversized = oversizedRead;
        QTimer::singleShot(0, operation, [operation, snapshot, oversized] {
            if (oversized) {
                operation->finish({OperationStatus::ProtocolError, 0,
                                   QStringLiteral("configuration.output_limit_exceeded")});
                return;
            }
            operation->reportStandardOutput(snapshot);
            operation->finish({OperationStatus::Succeeded, 0, QStringLiteral("fake.read")});
        });
        return operation;
    }

    PlatformOperation *writeConfiguration(
        const QByteArray &newContents, const Sha256Digest &expectedDigest) override
    {
        auto *operation = new PlatformOperation(this);
        const QByteArray expected = expectedDigest.hex();
        QTimer::singleShot(0, operation, [this, operation, newContents, expected] {
            const QByteArray actual =
                QCryptographicHash::hash(contents, QCryptographicHash::Sha256).toHex();
            if (actual != expected) {
                operation->reportStandardError(QByteArray("UNTRUSTED_HELPER_OUTPUT"));
                operation->finish({OperationStatus::Failed, 9, QStringLiteral("fake.stale")});
                return;
            }
            previousContents = contents;
            contents = newContents;
            backupCreated = true;
            operation->reportStandardOutput(QByteArray("saved"));
            operation->finish({OperationStatus::Succeeded, 0, QStringLiteral("fake.saved")});
        });
        return operation;
    }

    PlatformOperation *rollbackConfiguration() override
    {
        auto *operation = new PlatformOperation(this);
        QTimer::singleShot(0, operation, [this, operation] {
            contents = previousContents;
            operation->finish({OperationStatus::Succeeded, 0, QStringLiteral("fake.rollback")});
        });
        return operation;
    }
};

class TestProfileController final : public QObject
{
    Q_OBJECT
private slots:
    void rejectsOversizedConfigOutput();
    void loadsAndSavesProfileMutations();
    void removingAnotherProfilePreservesSelection();
    void confirmedReloadDiscardsUnsavedChanges();
    void staleConfigIsNeverOverwritten();
};

void TestProfileController::rejectsOversizedConfigOutput()
{
    FakeConfigurationOperations operations;
    operations.oversizedRead = true;
    ProfileController controller(&operations);
    controller.read();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);
    QVERIFY(!controller.loaded());
    QCOMPARE(controller.status(), QStringLiteral("Не удалось прочитать конфигурацию (код 0)."));
}

void TestProfileController::loadsAndSavesProfileMutations()
{
    FakeConfigurationOperations operations;
    ProfileController controller(&operations);
    controller.read();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);
    QVERIFY2(controller.loaded(), qPrintable(controller.status()));
    QCOMPARE(controller.profiles().size(), 1);

    controller.add({QStringLiteral("--filter-tcp=443"),
                    QStringLiteral("--payload=tls_client_hello")});
    QCOMPARE(controller.profiles().size(), 2);
    controller.copy(1);
    QCOMPARE(controller.profiles().size(), 3);
    controller.edit(2, {QStringLiteral("--filter-tcp=8443")});
    controller.remove(1);
    QCOMPARE(controller.profiles().size(), 2);
    QVERIFY(controller.dirty());

    controller.save();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);
    QVERIFY2(!controller.dirty(), qPrintable(controller.status()));
    QVERIFY(operations.contents.contains("--filter-tcp=80"));
    QVERIFY(operations.contents.contains("--filter-tcp=8443"));
    QVERIFY(!operations.contents.contains("--filter-tcp=443"));
    QVERIFY(operations.backupCreated);
}

void TestProfileController::removingAnotherProfilePreservesSelection()
{
    FakeConfigurationOperations operations;
    ProfileController controller(&operations);
    controller.read();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);
    QVERIFY(controller.loaded());
    controller.add({QStringLiteral("--filter-tcp=443")});
    controller.setSelectedIndex(0);
    controller.remove(1);
    QCOMPARE(controller.selectedIndex(), 0);
    QCOMPARE(controller.profiles().size(), 1);
}

void TestProfileController::confirmedReloadDiscardsUnsavedChanges()
{
    FakeConfigurationOperations operations;
    ProfileController controller(&operations);
    controller.read();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);
    QVERIFY(controller.loaded());
    controller.add({QStringLiteral("--filter-tcp=443")});
    QVERIFY(controller.dirty());
    controller.read();
    QVERIFY(controller.dirty());
    QCOMPARE(controller.profiles().size(), 2);
    controller.read(true);
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);
    QVERIFY2(!controller.dirty(), qPrintable(controller.status()));
    QCOMPARE(controller.profiles().size(), 1);
}

void TestProfileController::staleConfigIsNeverOverwritten()
{
    FakeConfigurationOperations operations;
    ProfileController controller(&operations);
    controller.read();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);
    QVERIFY(controller.loaded());
    operations.contents.append("# external change\n");
    const QByteArray externallyChanged = operations.contents;
    controller.add({QStringLiteral("--filter-tcp=443")});
    controller.save();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);
    QVERIFY(controller.dirty());
    QCOMPARE(controller.status(), QStringLiteral("Запись не выполнена (код 9)."));
    QVERIFY(!controller.status().contains(QStringLiteral("UNTRUSTED_HELPER_OUTPUT")));
    QCOMPARE(operations.contents, externallyChanged);
}

QTEST_GUILESS_MAIN(TestProfileController)
#include "test_profilecontroller.moc"
