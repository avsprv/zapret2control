#pragma once

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QString>

#include "platform/privilegedoperations.h"

class ListsController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString content READ content NOTIFY contentChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString loadedName READ loadedName NOTIFY loadedNameChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY dirtyChanged)
    Q_PROPERTY(bool editable READ editable NOTIFY editableChanged)

public:
    explicit ListsController(Zapret::Platform::HostListOperations *operations,
                             QObject *parent = nullptr);

    QString content() const;
    QString status() const;
    QString loadedName() const;
    bool busy() const;
    bool loaded() const;
    bool dirty() const;
    bool editable() const;

    Q_INVOKABLE bool isEditableName(const QString &name) const;
    Q_INVOKABLE void readList(const QString &name);
    Q_INVOKABLE void setContent(const QString &content);
    Q_INVOKABLE void saveList();

signals:
    void contentChanged();
    void statusChanged();
    void loadedNameChanged();
    void busyChanged();
    void loadedChanged();
    void dirtyChanged();
    void editableChanged();

private:
    bool isAllowedName(const QString &name) const;
    QString validateHostlist(const QString &content) const;
    void setStatus(const QString &status);
    void setBusy(bool busy);
    void setLoaded(bool loaded);
    void setDirty(bool dirty);
    void setContentInternal(const QString &content);
    void setLoadedName(const QString &name);
    void updateEditable();
    QString operationErrorText(Zapret::Platform::OperationStatus status,
                                bool writing) const;

    Zapret::Platform::HostListOperations *m_operations = nullptr;
    QPointer<Zapret::Platform::PlatformOperation> m_operation;
    QString m_content;
    QString m_status;
    QString m_loadedName;
    QByteArray m_output;
    bool m_outputExceeded = false;
    bool m_busy = false;
    bool m_loaded = false;
    bool m_dirty = false;
    bool m_editable = false;
};
