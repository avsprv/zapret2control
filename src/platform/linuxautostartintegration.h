#pragma once

#include "platform/autostartintegration.h"

class LinuxAutostartIntegration final : public AutostartIntegration
{
    Q_OBJECT

public:
    explicit LinuxAutostartIntegration(QObject *parent = nullptr,
                                       const QString &autostartDirectory = {},
                                       const QString &executablePath = {});

    bool enabled() const override;
    QString errorMessage() const override;

    void setEnabled(bool enabled) override;

private:
    QString entryPath() const;
    bool isManagedEntry() const;
    void setError(const QString &message);

    QString m_autostartDirectory;
    QString m_executablePath;
    QString m_errorMessage;
};
