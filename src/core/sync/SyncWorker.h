#pragma once

#include "SyncKey.h"

#include <QByteArray>
#include <QObject>
#include <QString>

/**
 * Runs every blocking synchronization network/IO operation.
 *
 * A SyncWorker is moved onto a dedicated QThread owned by SyncController, so all
 * GitHub round-trips and file I/O happen off the GUI thread. Each public slot
 * performs one operation and reports back through a corresponding signal, which
 * is queued to the controller (and therefore to the UI) automatically.
 */
class SyncWorker : public QObject
{
    Q_OBJECT

public:
    explicit SyncWorker(QObject* parent = nullptr);

public slots:
    void runCreate(const SyncKey& key, const QString& token, const QString& description,
                   const QString& filename, const QByteArray& encrypted);
    void runPush(const SyncKey& key, const QString& token, const QString& filename,
                 const QByteArray& encrypted);
    void runPull(const SyncKey& key, const QString& token, const QString& filename);
    void runTestToken(const QString& token);
    void runAbort();

signals:
    void createFinished(bool ok, const QString& gistId, const QString& error);
    void pushFinished(bool ok, const QString& error);
    void pullFinished(bool ok, bool notFound, const QString& body, const QString& error);
    void testFinished(bool ok, const QString& error);
};
