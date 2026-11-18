#include <QtTest>

#include <QCryptographicHash>
#include <QFile>
#include <QTemporaryDir>
#include <QTimer>

#include "qml/configcontroller.h"

using namespace Zapret::Platform;

class FakeConfigurationOperations final : public QObject, public ConfigurationOperations
{
    Q_OBJECT

public:
    explicit FakeConfigurationOperations(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    QByteArray contents = QByteArray("NFQWS2_ENABLE=1\n");
    QByteArray previousContents = contents;
    bool failNextWrite = false;
    QByteArray failureDiagnostic;
    bool denyNextOperation = false;

    PlatformOperation *readConfiguration() override
    {
        auto *operation = new PlatformOperation(this);
        const QByteArray snapshot = contents;
        QTimer::singleShot(0, operation, [operation, snapshot] {
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
            if (denyNextOperation) {
                denyNextOperation = false;
                operation->finish({OperationStatus::AuthorizationDenied, 126,
                                   QStringLiteral("fake.denied")});
                return;
            }

            const QByteArray actual =
                QCryptographicHash::hash(contents, QCryptographicHash::Sha256).toHex();
            if (actual != expected) {
                operation->reportStandardError(QByteArray("stale config"));
                operation->finish({OperationStatus::Failed, 9, QStringLiteral("fake.stale")});
                return;
            }
            if (failNextWrite) {
                failNextWrite = false;
                if (!failureDiagnostic.isEmpty())
                    operation->reportStandardError(failureDiagnostic);
                operation->finish({OperationStatus::Failed, 5, QStringLiteral("fake.write_failed")});
                return;
            }

            previousContents = contents;
            contents = newContents;
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
            operation->reportStandardOutput(QByteArray("rollback-ok"));
            operation->finish({OperationStatus::Succeeded, 0, QStringLiteral("fake.rollback")});
        });
        return operation;
    }
};

class TestConfigController final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void readsConfiguration();
    void savesValidatedConfiguration();
    void rejectsInvalidShellSyntax();
    void reportsAuthorizationCancellation();
    void doesNotExposeHelperDiagnostics();
    void rollsBackAndReloadsConfiguration();
};

void TestConfigController::readsConfiguration()
{
    FakeConfigurationOperations operations;
    ConfigController controller(&operations);

    controller.read();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);

    QVERIFY(controller.loaded());
    QCOMPARE(controller.configText(), QString::fromUtf8(operations.contents));
    QVERIFY(controller.status().contains(QStringLiteral("Конфигурация загружена")));
}

void TestConfigController::savesValidatedConfiguration()
{
    FakeConfigurationOperations operations;
    ConfigController controller(&operations);
    controller.read();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);

    controller.setConfigText(QStringLiteral("NFQWS2_ENABLE=1\n# updated\n"));
    controller.save();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);

    QCOMPARE(operations.contents, controller.configText().toUtf8());
    QCOMPARE(controller.status(), QStringLiteral("saved"));
}

void TestConfigController::rejectsInvalidShellSyntax()
{
    FakeConfigurationOperations operations;
    ConfigController controller(&operations);
    controller.read();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);

    controller.setConfigText(QStringLiteral("if then\n"));
    controller.save();

    QVERIFY(!controller.busy());
    QVERIFY(controller.status().contains(QStringLiteral("Сохранение отменено")));
    QCOMPARE(operations.contents, QByteArray("NFQWS2_ENABLE=1\n"));
}

void TestConfigController::reportsAuthorizationCancellation()
{
    FakeConfigurationOperations operations;
    ConfigController controller(&operations);
    controller.read();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);

    operations.denyNextOperation = true;
    controller.setConfigText(QStringLiteral("NFQWS2_ENABLE=0\n"));
    controller.save();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);

    QVERIFY(controller.status().contains(QStringLiteral("авторизация не выполнена")));
    QCOMPARE(operations.contents, QByteArray("NFQWS2_ENABLE=1\n"));
}

void TestConfigController::doesNotExposeHelperDiagnostics()
{
    FakeConfigurationOperations operations;
    ConfigController controller(&operations);
    controller.read();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);

    const QString untrustedText = QStringLiteral("UNTRUSTED_HELPER_OUTPUT");
    operations.failNextWrite = true;
    operations.failureDiagnostic = untrustedText.toUtf8();
    controller.setConfigText(QStringLiteral("NFQWS2_ENABLE=0\\n"));
    controller.save();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);

    QVERIFY(!controller.status().contains(untrustedText));
    QCOMPARE(controller.status(), QStringLiteral("Операция не выполнена."));
    QCOMPARE(operations.contents, QByteArray("NFQWS2_ENABLE=1\\n"));
}

void TestConfigController::rollsBackAndReloadsConfiguration()
{
    FakeConfigurationOperations operations;
    ConfigController controller(&operations);
    controller.read();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);

    controller.setConfigText(QStringLiteral("NFQWS2_ENABLE=0\n"));
    controller.save();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);
    QCOMPARE(operations.contents, QByteArray("NFQWS2_ENABLE=0\n"));

    controller.rollback();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);
    QCOMPARE(controller.configText(), QStringLiteral("NFQWS2_ENABLE=1\n"));
    QCOMPARE(operations.contents, QByteArray("NFQWS2_ENABLE=1\n"));
}

QTEST_GUILESS_MAIN(TestConfigController)

#include "test_configcontroller.moc"
