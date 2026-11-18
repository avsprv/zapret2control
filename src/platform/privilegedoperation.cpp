#include "privilegedoperation.h"

#include <QThread>

namespace Zapret::Platform {

PlatformOperation::PlatformOperation(QObject *parent)
    : QObject(parent)
{
}

bool PlatformOperation::isFinished() const
{
    return m_finished;
}

void PlatformOperation::reportStandardOutput(const QByteArray &data)
{
    Q_ASSERT(QThread::currentThread() == thread());

    if (!m_finished && !data.isEmpty())
        emit standardOutputReceived(data);
}

void PlatformOperation::reportStandardError(const QByteArray &data)
{
    Q_ASSERT(QThread::currentThread() == thread());

    if (!m_finished && !data.isEmpty())
        emit standardErrorReceived(data);
}

void PlatformOperation::finish(const OperationResult &result)
{
    Q_ASSERT(QThread::currentThread() == thread());

    if (m_finished)
        return;

    m_finished = true;
    emit finished(result);
}

} // namespace Zapret::Platform
