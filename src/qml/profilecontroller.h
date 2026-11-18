#pragma once

#include <QObject>
#include <QByteArray>
#include <QVariant>
#include <QVector>

#include "profileanalyzer.h"
#include "platform/privilegedoperations.h"

class ProfileController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList profiles READ profiles NOTIFY profilesChanged)
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex NOTIFY selectedIndexChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY dirtyChanged)
public:
    explicit ProfileController(Zapret::Platform::ConfigurationOperations *operations,
                               QObject *parent = nullptr);
    QVariantList profiles() const;
    int selectedIndex() const;
    void setSelectedIndex(int index);
    QString status() const;
    bool busy() const;
    bool loaded() const;
    bool dirty() const;
    Q_INVOKABLE void read(bool discardChanges = false);
    Q_INVOKABLE void add(const QStringList &arguments);
    Q_INVOKABLE void edit(int index, const QStringList &arguments);
    Q_INVOKABLE void copy(int index);
    Q_INVOKABLE void remove(int index);
    Q_INVOKABLE void save();
signals:
    void profilesChanged();
    void selectedIndexChanged();
    void statusChanged();
    void busyChanged();
    void loadedChanged();
    void dirtyChanged();
private:
    void setStatus(const QString &value);
    void setBusy(bool value);
    void setDirty(bool value);
    void setLoaded(bool value);
    bool validArguments(const QStringList &arguments, QString *error) const;
    void startOperation(Zapret::Platform::PlatformOperation *operation, bool writing,
                        const QByteArray &input = {});
    void finishOperation(Zapret::Platform::PlatformOperation *operation, bool writing,
                         const Zapret::Platform::OperationResult &result);
    Zapret::Platform::ConfigurationOperations *m_operations = nullptr;
    Zapret::Platform::PlatformOperation *m_operation = nullptr;
    QByteArray m_configBytes;
    QByteArray m_output;
    QByteArray m_configInput;
    QString m_config;
    QVector<ProfileAnalyzer::Profile> m_profiles;
    QString m_status;
    int m_selectedIndex = -1;
    bool m_busy = false;
    bool m_loaded = false;
    bool m_dirty = false;
};
