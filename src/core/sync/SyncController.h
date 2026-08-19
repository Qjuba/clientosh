#pragma once

#include "SyncKey.h"
#include "SyncPayload.h"
#include "SyncWorker.h"

#include <QByteArray>
#include <QObject>
#include <QString>

#include <functional>

class QThread;
class QTimer;

/**
 * GUI-thread orchestrator for the clientosh GitHub Gist synchronization.
 *
 * The controller holds the synchronization identity and performs all data
 * transforms (encrypt/decrypt, version/payload reconciliation) on the calling
 * thread, while delegating every blocking network operation to a SyncWorker that
 * runs on a dedicated QThread. Results are delivered back via queued signals, so
 * the UI never blocks.
 *
 * Serialization bridging:
 *   - `setDataProvider(serialize, apply)` lets the owner supply a hook that
 *     captures the current app data as a SyncPayload (or nullopt) and applies a
 *     synced payload back. This keeps the sync layer decoupled from the app.
 *
 * All successful writes bump the payload revision (a "commit"), so other
 * devices can detect and pull the update during their polling cycle.
 */
class SyncController : public QObject
{
    Q_OBJECT

public:
    using SerializeFn = std::function<QByteArray()>;            // raw JSON payload bytes
    using ApplyFn = std::function<bool(const QByteArray&)>;     // apply incoming payload

    enum class State {
        Disabled,   // sync off, local data only
        Connecting, // setting up / joining in progress
        Active      // connected and auto-syncing
    };
    Q_ENUM(State)

    explicit SyncController(QObject* parent = nullptr);
    ~SyncController() override;

    State state() const { return m_state; }
    QString syncKeyString() const; // current portable sync key (empty when disabled)
    bool hasToken() const;

    /** Provide how the current local data is serialized / applied. */
    void setDataProvider(SerializeFn serialize, ApplyFn apply);
    /** Provide the stable local device id + human label (hostname). */
    void setDeviceId(const QString& id, const QString& label);

    // ---- Lifecycle ---------------------------------------------------------
    /** Start a brand-new sync on Computer 1: generates keys, creates gist, pushes. */
    void createSync(const QString& token, const QString& gistDescription);
    /** Join an existing sync on Computer 2 from a copied key + local token. */
    void joinSync(const QString& syncKeyText, const QString& token);
    /**
     * Reconnect to a previously configured sync on this machine, restoring the
     * last known revision so an empty/stale gist cannot wipe local data.
     */
    void restoreExisting(const QString& syncKeyText, const QString& token);
    /** Test whether a GitHub token is valid (async). err slot reports result. */
    void testToken(const QString& token);
    /** Turn sync off. Keeps local data untouched; stops polling and worker. */
    void disable();
    /**
     * Pause or resume automatic polling/push without forgetting the sync key.
     * `pause(true)` stops the timer; `pause(false)` resumes it when Active.
     */
    void setPaused(bool paused);
    bool isPaused() const { return m_paused; }

    /** Immediately check the remote gist and pull newer data (async). */
    void pullNow();
    /** Immediately push the current local data as a new revision (async). */
    void pushNow();
    /** Pull remote changes, then push the current local snapshot (async). */
    void syncNow();

    /** Configure how often (seconds) the controller polls for remote changes. */
    void setPollIntervalSec(int seconds);

signals:
    void stateChanged(SyncController::State state);
    void statusMessage(const QString& message);
    void errorOccurred(const QString& message);
    void dataUpdated(); // a pulled payload was applied locally

signals:
    // Internal cross-thread requests to the worker (queued automatically).
    void requestCreate(const SyncKey& key, const QString& token, const QString& description,
                       const QString& filename, const QByteArray& encrypted);
    void requestPush(const SyncKey& key, const QString& token, const QString& filename,
                     const QByteArray& encrypted);
    void requestPull(const SyncKey& key, const QString& token, const QString& filename);
    void requestTestToken(const QString& token);

private slots:
    void onCreateFinished(bool ok, const QString& gistId, const QString& error);
    void onPushFinished(bool ok, const QString& error);
    void onPullFinished(bool ok, bool notFound, const QString& body, const QString& error);
    void onTestFinished(bool ok, const QString& error);
    void onPollTick();

private:
    enum class PendingOp { None, Create, Join, Push, Pull, Test };

    void setState(State st);
    void startWorker();
    void stopWorker();
    void push();
    void beginJoin(const QString& syncKeyText, const QString& token, bool restoring);
    void reconcileFromRemote(const SyncPayload& remote, bool joining);
    SyncPayload currentLocalPayload() const;
    QByteArray serializeWithFraming();
    void startPollTimer();
    void flushQueuedPush();

    QThread* m_thread = nullptr;
    SyncWorker* m_worker = nullptr;

    SyncKey m_key;          // session identity / data key
    QString m_token;        // local-only GitHub token (kept out of sync data)
    QString m_gistDesc;     // display description for the created gist
    QString m_filename;     // gist file slot name (derived from sync uuid)
    bool m_tokenLocal = false;

    State m_state = State::Disabled;
    PendingOp m_pending = PendingOp::None;

    SerializeFn m_serialize;
    ApplyFn m_apply;
    QString m_deviceId = QStringLiteral("device");
    QString m_deviceLabel;

    QTimer* m_pollTimer = nullptr;
    int m_pollIntervalSec = 60;
    int m_lastKnownRev = 0;
    bool m_connectedOnce = false;
    bool m_paused = false;
    bool m_pushQueued = false;     // local change arrived while another op ran
    bool m_syncNowQueued = false;  // Sync now: pull, then push
};

Q_DECLARE_METATYPE(SyncController::State)
