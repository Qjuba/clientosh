#include "SyncWorker.h"

#include "SyncGitHub.h"
#include "SyncKey.h"

SyncWorker::SyncWorker(QObject* parent)
    : QObject(parent)
{
}

void SyncWorker::runCreate(const SyncKey& key, const QString& token,
                           const QString& description, const QString& filename,
                           const QByteArray& encrypted)
{
    Q_UNUSED(key);
    const SyncGitHub::CreateResult r = SyncGitHub::createGist(token, description,
                                                              filename, encrypted);
    emit createFinished(r.ok, r.gistId, r.error);
}

void SyncWorker::runPush(const SyncKey& key, const QString& token,
                         const QString& filename, const QByteArray& encrypted)
{
    Q_UNUSED(key);
    const SyncGitHub::WriteResult r = SyncGitHub::updateGist(token, key.gistId,
                                                             filename, encrypted);
    emit pushFinished(r.ok, r.error);
}

void SyncWorker::runPull(const SyncKey& key, const QString& token,
                         const QString& filename)
{
    const SyncGitHub::ReadResult r = SyncGitHub::readGist(token, key.gistId, filename);
    emit pullFinished(r.ok, r.notFound, r.body, r.error);
}

void SyncWorker::runTestToken(const QString& token)
{
    QString error;
    const bool ok = SyncGitHub::checkToken(token, &error);
    emit testFinished(ok, error);
}

void SyncWorker::runAbort()
{
    // No-op: blocking requests complete and are ignored by the controller.
}
