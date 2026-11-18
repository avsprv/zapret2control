#include <QtTest>

#include <QTimer>

#include "listscontroller.h"

using namespace Zapret::Platform;

class FakeHostListOperations final : public QObject, public HostListOperations
{
    Q_OBJECT

public:
    QByteArray readContents = QByteArray("example.org\n");
    OperationResult readResult {
        OperationStatus::Succeeded, 0, QStringLiteral("helper.completed")
    };
    OperationResult writeResult {
        OperationStatus::Succeeded, 0, QStringLiteral("helper.completed")
    };
    HostListId lastReadId = HostListId::ZapretHostsUser;
    HostListId lastWriteId = HostListId::ZapretHostsUser;
    QByteArray lastWriteContents;
    int readCalls = 0;
    int writeCalls = 0;
    bool emitUntrustedDiagnostics = true;
    bool returnNull = false;

    PlatformOperation *readList(HostListId listId) override
    {
        ++readCalls;
        lastReadId = listId;
        if (returnNull)
            return nullptr;

        auto *operation = new PlatformOperation(this);
        const QByteArray contents = readContents;
        const OperationResult result = readResult;
        const bool emitDiagnostics = emitUntrustedDiagnostics;
        QTimer::singleShot(0, operation, [operation, contents, result, emitDiagnostics] {
            if (result.succeeded()
                && result.diagnosticCode != QLatin1String("hostlist.not_found")) {
                operation->reportStandardOutput(contents);
            }
            if (emitDiagnostics) {
                operation->reportStandardError(QByteArray("UNTRUSTED_HELPER_OUTPUT"));
            }
            operation->finish(result);
        });
        return operation;
    }

    PlatformOperation *writeList(HostListId listId, const QByteArray &contents) override
    {
        ++writeCalls;
        lastWriteId = listId;
        lastWriteContents = contents;
        if (returnNull)
            return nullptr;

        auto *operation = new PlatformOperation(this);
        const OperationResult result = writeResult;
        const bool emitDiagnostics = emitUntrustedDiagnostics;
        QTimer::singleShot(0, operation, [operation, result, emitDiagnostics] {
            if (emitDiagnostics) {
                operation->reportStandardOutput(QByteArray("UNTRUSTED_HELPER_OUTPUT"));
                operation->reportStandardError(QByteArray("UNTRUSTED_HELPER_ERROR"));
            }
            operation->finish(result);
        });
        return operation;
    }
};

class TestListsController final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void rejectsUnknownListName();
    void exposesOnlySourceListsAsEditable();
    void readsListThroughTypedOperation();
    void treatsMissingListAsCreatableEmptyList();
    void validatesAndWritesOnlyChangedSourceList();
    void keepsDerivedListsReadOnly();
    void doesNotExposeHelperDiagnostics();
    void rejectsInvalidUtf8();
    void rejectsOversizedOutput();
    void resetsBusyStateWhenProviderCannotStart();
};

void TestListsController::rejectsUnknownListName()
{
    FakeHostListOperations operations;
    ListsController controller(&operations);
    controller.readList(QStringLiteral("../../etc/passwd"));
    QVERIFY(!controller.busy());
    QVERIFY(controller.status().contains(QStringLiteral("Недопустимое")));
    QCOMPARE(operations.readCalls, 0);
}

void TestListsController::exposesOnlySourceListsAsEditable()
{
    FakeHostListOperations operations;
    ListsController controller(&operations);
    QVERIFY(controller.isEditableName(QStringLiteral("zapret-hosts-user.txt")));
    QVERIFY(controller.isEditableName(QStringLiteral("zapret-hosts-user-exclude.txt")));
    QVERIFY(controller.isEditableName(QStringLiteral("zapret-hosts-user-ipban.txt")));
    QVERIFY(!controller.isEditableName(QStringLiteral("zapret-ip-user.txt")));
    QVERIFY(!controller.isEditableName(QStringLiteral("zapret-ip-user6.txt")));
}

void TestListsController::readsListThroughTypedOperation()
{
    FakeHostListOperations operations;
    ListsController controller(&operations);
    controller.readList(QStringLiteral("zapret-hosts-user.txt"));

    QTRY_VERIFY_WITH_TIMEOUT(controller.loaded(), 1000);
    QCOMPARE(operations.lastReadId, HostListId::ZapretHostsUser);
    QCOMPARE(controller.content(), QStringLiteral("example.org\n"));
    QVERIFY(controller.editable());
    QVERIFY(!controller.dirty());
    QVERIFY(!controller.busy());
}

void TestListsController::treatsMissingListAsCreatableEmptyList()
{
    FakeHostListOperations operations;
    operations.readContents.clear();
    operations.readResult = {
        OperationStatus::Succeeded, 0, QStringLiteral("hostlist.not_found")
    };
    ListsController controller(&operations);
    controller.readList(QStringLiteral("zapret-hosts-user-exclude.txt"));

    QTRY_VERIFY_WITH_TIMEOUT(controller.loaded(), 1000);
    QCOMPARE(operations.lastReadId, HostListId::ZapretHostsUserExclude);
    QVERIFY(controller.content().isEmpty());
    QVERIFY(controller.editable());
    QVERIFY(controller.status().contains(QStringLiteral("ещё не создан")));
}

void TestListsController::validatesAndWritesOnlyChangedSourceList()
{
    FakeHostListOperations operations;
    ListsController controller(&operations);
    controller.readList(QStringLiteral("zapret-hosts-user.txt"));
    QTRY_VERIFY_WITH_TIMEOUT(controller.loaded(), 1000);

    controller.setContent(QStringLiteral("valid.example\n192.0.2.1/24\n"));
    QVERIFY(controller.dirty());
    controller.saveList();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);

    QCOMPARE(operations.writeCalls, 1);
    QCOMPARE(operations.lastWriteId, HostListId::ZapretHostsUser);
    QCOMPARE(operations.lastWriteContents, QByteArray("valid.example\n192.0.2.1/24\n"));
    QVERIFY(!controller.dirty());
    QCOMPARE(controller.status(), QStringLiteral("Список сохранён."));

    controller.setContent(QStringLiteral("invalid..domain\n"));
    QVERIFY(controller.dirty());
    controller.saveList();
    QVERIFY(!controller.busy());
    QCOMPARE(operations.writeCalls, 1);
    QVERIFY(controller.status().contains(QStringLiteral("Некорректный домен")));
}

void TestListsController::keepsDerivedListsReadOnly()
{
    FakeHostListOperations operations;
    ListsController controller(&operations);
    controller.readList(QStringLiteral("zapret-ip-user.txt"));
    QTRY_VERIFY_WITH_TIMEOUT(controller.loaded(), 1000);

    QVERIFY(!controller.editable());
    const QString original = controller.content();
    controller.setContent(QStringLiteral("changed.example\n"));
    QCOMPARE(controller.content(), original);
    QVERIFY(!controller.dirty());
    controller.saveList();
    QCOMPARE(operations.writeCalls, 0);
}

void TestListsController::doesNotExposeHelperDiagnostics()
{
    FakeHostListOperations operations;
    ListsController controller(&operations);
    controller.readList(QStringLiteral("zapret-hosts-user.txt"));
    QTRY_VERIFY_WITH_TIMEOUT(controller.loaded(), 1000);

    operations.writeResult = {
        OperationStatus::Failed, 17, QStringLiteral("helper.exit_failed")
    };
    controller.setContent(QStringLiteral("changed.example\n"));
    controller.saveList();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);

    QVERIFY(controller.dirty());
    QCOMPARE(controller.status(), QStringLiteral("Не удалось сохранить список."));
    QVERIFY(!controller.status().contains(QStringLiteral("UNTRUSTED_HELPER")));
}

void TestListsController::rejectsInvalidUtf8()
{
    FakeHostListOperations operations;
    operations.readContents = QByteArray("\xff\xfe", 2);
    ListsController controller(&operations);
    controller.readList(QStringLiteral("zapret-hosts-user.txt"));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);

    QVERIFY(!controller.loaded());
    QVERIFY(controller.status().contains(QStringLiteral("некорректный UTF-8")));
}

void TestListsController::rejectsOversizedOutput()
{
    FakeHostListOperations operations;
    operations.readContents = QByteArray(1024 * 1024 + 1, 'x');
    ListsController controller(&operations);
    controller.readList(QStringLiteral("zapret-hosts-user.txt"));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);

    QVERIFY(!controller.loaded());
    QVERIFY(controller.status().contains(QStringLiteral("превышает ограничение 1 MiB")));
}

void TestListsController::resetsBusyStateWhenProviderCannotStart()
{
    FakeHostListOperations operations;
    operations.returnNull = true;
    ListsController controller(&operations);

    controller.readList(QStringLiteral("zapret-hosts-user.txt"));
    QVERIFY(!controller.busy());
    QVERIFY(controller.status().contains(QStringLiteral("Не удалось прочитать список")));

    operations.returnNull = false;
    controller.readList(QStringLiteral("zapret-hosts-user.txt"));
    QTRY_VERIFY_WITH_TIMEOUT(controller.loaded(), 1000);
    controller.setContent(QStringLiteral("changed.example\n"));

    operations.returnNull = true;
    controller.saveList();
    QVERIFY(!controller.busy());
    QVERIFY(controller.dirty());
    QVERIFY(controller.status().contains(QStringLiteral("Не удалось сохранить список")));
}

QTEST_GUILESS_MAIN(TestListsController)

#include "test_listscontroller.moc"
