#include "SyncController.h"

#include "SyncConfig.h"
#include "SyncCrypto.h"
#include "SyncKey.h"
#include "SyncPayload.h"

#include <QDateTime>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <functional>
#include <stdexcept>

namespace {
inline QString b64url(const QByteArray& data)
{
    return QString::fromLatin1(data.toBase64(QByteArray::Base64UrlEncoding
                                             | QByteArray::OmitTrailingEquals));
}
} // namespace

SyncController::SyncController(QObject* parent)
    : QObject(parent)
{
    m_pollTimer = new QTimer(this);
    m_pollTimer->setTimerType(Qt::VeryCoarseTimer);
    m_pollTimer->setSingleShot(false);
    connect(m_pollTimer, &QTimer::timeout, this, &SyncController::onPollTick);
}

SyncController::~SyncController()
{
    stopWorker();
}

void SyncController::setDataProvider(SerializeFn serialize, ApplyFn apply)
{
    m_serialize = std::move(serialize);
    m_apply = std::move(apply);
}

void SyncController::setDeviceId(const QString& id, const QString& label)
{
    m_deviceId = id;
    m_deviceLabel = label;
}

void SyncController::setState(State st)
{
    if (m_state == st) {
        return;
    }
    m_state = st;
    emit stateChanged(st);
}

QString SyncController::syncKeyString() const
{
    return m_state == State::Disabled ? QString() : SyncKeyCodec::encode(m_key);
}

bool SyncController::hasToken() const
{
    return m_tokenLocal && !m_token.isEmpty();
}

void SyncController::setPollIntervalSec(int seconds)
{
    m_pollIntervalSec = std::max(10, seconds);
    if (m_state == State::Active) {
        startPollTimer();
    }
}

void SyncController::startPollTimer()
{
    m_pollTimer->stop();
    m_pollTimer->start(m_pollIntervalSec * 1000);
}

void SyncController::startWorker()
{
    if (m_thread) {
        return;
    }
    m_thread = new QThread(this);
    m_worker = new SyncWorker;
    m_worker->moveToThread(m_thread);

    connect(this, &SyncController::requestCreate, m_worker, &SyncWorker::runCreate);
    connect(this, &SyncController::requestPush, m_worker, &SyncWorker::runPush);
    connect(this, &SyncController::requestPull, m_worker, &SyncWorker::runPull);
    connect(this, &SyncController::requestTestToken, m_worker, &SyncWorker::runTestToken);

    connect(m_worker, &SyncWorker::createFinished, this, &SyncController::onCreateFinished);
    connect(m_worker, &SyncWorker::pushFinished, this, &SyncController::onPushFinished);
    connect(m_worker, &SyncWorker::pullFinished, this, &SyncController::onPullFinished);
    connect(m_worker, &SyncWorker::testFinished, this, &SyncController::onTestFinished);

    m_thread->start();
}

void SyncController::stopWorker()
{
    if (m_thread) {
        m_thread->quit();
        m_thread->wait(3000);
        if (m_worker) {
            m_worker->deleteLater();
            m_worker = nullptr;
        }
        m_thread->wait(500);
        delete m_thread;
        m_thread = nullptr;
    }
    m_pending = PendingOp::None;
}

// ---- Lifecycle ---------------------------------------------------------

void SyncController::createSync(const QString& token, const QString& gistDescription)
{
    if (token.trimmed().isEmpty()) {
        emit errorOccurred(QStringLiteral("A GitHub token is required to create a sync."));
        return;
    }
    startWorker();
    if (m_state == State::Connecting) {
        return;
    }
    setState(State::Connecting);
    m_token = token;
    m_tokenLocal = true;
    m_gistDesc = gistDescription.trimmed().isEmpty()
        ? QStringLiteral("clientosh saved-sessions sync")
        : gistDescription.trimmed();

    // First-time setup on Computer 1: generate all local crypto keys + identity.
    m_key = SyncCrypto::generateSyncKey();
    m_key.token = token.toUtf8(); // embed the token so the key fully joins the sync
    m_filename = QStringLiteral("clientosh-sync-%1.json").arg(b64url(m_key.syncUuid));

    // Encrypt the current local data as the initial revision.
    m_lastKnownRev = 0;
    SyncConfig::setLastKnownRev(0);
    const QByteArray encrypted = SyncCrypto::encryptPayload(m_key, serializeWithFraming()).toBase64();

    m_pending = PendingOp::Create;
    emit requestCreate(m_key, m_token, m_gistDesc, m_filename, encrypted);
}

void SyncController::joinSync(const QString& syncKeyText, const QString& token)
{
    const SyncKey parsed = SyncKeyCodec::decode(syncKeyText);
    if (!parsed.isValid()) {
        emit errorOccurred(QStringLiteral("That sync key is malformed. Please copy the exact key and try again."));
        return;
    }
    // The token may come embedded in the key (preferred) or be passed explicitly.
    QString effectiveToken = token;
    if (effectiveToken.trimmed().isEmpty() && !parsed.token.isEmpty()) {
        effectiveToken = QString::fromUtf8(parsed.token);
    }
    if (effectiveToken.trimmed().isEmpty()) {
        emit errorOccurred(QStringLiteral("A GitHub token is required to read the synchronized gist."));
        return;
    }
    startWorker();
    if (m_state == State::Connecting) {
        return;
    }
    setState(State::Connecting);
    m_key = parsed;
    m_key.token = effectiveToken.toUtf8(); // retain token for future sync key persistence
    m_token = effectiveToken;
    m_tokenLocal = true;
    m_gistDesc.clear();
    m_filename = QStringLiteral("clientosh-sync-%1.json").arg(b64url(m_key.syncUuid));

    m_pending = PendingOp::Join;
    emit requestPull(m_key, m_token, m_filename);
}

void SyncController::testToken(const QString& token)
{
    if (token.trimmed().isEmpty()) {
        emit errorOccurred(QStringLiteral("Enter a GitHub token to test."));
        return;
    }
    startWorker();
    m_pending = PendingOp::Test;
    emit requestTestToken(token);
}

void SyncController::disable()
{
    m_pollTimer->stop();
    setState(State::Disabled);
    m_key = SyncKey{};
    m_token.clear();
    m_tokenLocal = false;
    m_gistDesc.clear();
    m_filename.clear();
    m_lastKnownRev = 0;
    m_pending = PendingOp::None;
    stopWorker();
    emit statusMessage(QStringLiteral("Synchronization disabled. Local data is unchanged."));
}

void SyncController::pullNow()
{
    if (m_state == State::Disabled || m_key.gistId.isEmpty() || !m_tokenLocal) {
        return;
    }
    if (m_pending != PendingOp::None) {
        return; // a sync op is already in flight
    }
    startWorker();
    m_pending = PendingOp::Pull;
    emit requestPull(m_key, m_token, m_filename);
}

void SyncController::pushNow()
{
    if (m_state == State::Disabled || m_key.gistId.isEmpty() || !m_tokenLocal) {
        return;
    }
    push();
}

void SyncController::push()
{
    if (m_pending != PendingOp::None) {
        return;
    }
    startWorker();
    const QByteArray encrypted = SyncCrypto::encryptPayload(m_key, serializeWithFraming()).toBase64();
    m_pending = PendingOp::Push;
    emit requestPush(m_key, m_token, m_filename, encrypted);
}

void SyncController::onPollTick()
{
    if (m_state == State::Disabled) {
        return;
    }
    pullNow();
}

// ---- Result handlers ---------------------------------------------------

void SyncController::onCreateFinished(bool ok, const QString& gistId, const QString& error)
{
    if (m_pending != PendingOp::Create) {
        return;
    }
    m_pending = PendingOp::None;
    if (!ok) {
        setState(State::Disabled);
        emit errorOccurred(QStringLiteral("Could not create the sync gist: %1").arg(error));
        return;
    }
    m_key.gistId = gistId;
    SyncConfig::setSyncKeyText(SyncKeyCodec::encode(m_key));
    m_lastKnownRev = std::max(m_lastKnownRev, 1);
    SyncConfig::setLastKnownRev(m_lastKnownRev);
    m_connectedOnce = true;
    setState(State::Active);
    startPollTimer();
    emit statusMessage(QStringLiteral("Sync created. Share the key below with other devices."));
}

void SyncController::onPushFinished(bool ok, const QString& error)
{
    if (m_pending != PendingOp::Push) {
        return;
    }
    m_pending = PendingOp::None;
    if (!ok) {
        emit errorOccurred(QStringLiteral("Upload failed: %1").arg(error));
        return;
    }
    // The payload that was pushed carried rev == old+1; adopt it as known-good.
    ++m_lastKnownRev;
    SyncConfig::setLastKnownRev(m_lastKnownRev);
    emit statusMessage(QStringLiteral("Changes uploaded."));
}

void SyncController::onPullFinished(bool ok, bool notFound, const QString& body,
                                    const QString& error)
{
    const PendingOp op = m_pending;
    m_pending = PendingOp::None;

    if (!ok) {
        if (op == PendingOp::Join) {
            setState(State::Disabled);
            emit errorOccurred(QStringLiteral("Could not join the sync: %1").arg(error));
        } else {
            emit errorOccurred(QStringLiteral("Sync check failed: %1").arg(error));
        }
        return;
    }

    // Decrypt the remote payload (gist content is base64-encoded ciphertext).
    SyncPayload remote;
    try {
        const QByteArray cipher = QByteArray::fromBase64(body.toLatin1());
        const QByteArray plain = SyncCrypto::decryptPayload(m_key, cipher);
        bool parsedOk = false;
        remote = SyncPayloadCodec::fromJson(plain, &parsedOk);
        if (!parsedOk) {
            throw std::runtime_error("payload parse");
        }
    } catch (...) {
        emit errorOccurred(QStringLiteral("Received data could not be decrypted. The sync key may be wrong."));
        return;
    }

    reconcileFromRemote(remote);

    if (op == PendingOp::Join) {
        setState(State::Active);
        startPollTimer();
        emit statusMessage(QStringLiteral("Connected to an existing sync."));
    } else {
        emit statusMessage(QStringLiteral("Synchronized."));
    }
}

void SyncController::onTestFinished(bool ok, const QString& error)
{
    m_pending = PendingOp::None;
    if (ok) {
        emit statusMessage(QStringLiteral("GitHub token is valid."));
    } else {
        emit errorOccurred(QStringLiteral("GitHub token rejected: %1").arg(error));
    }
}

// ---- Reconciliation ----------------------------------------------------

void SyncController::reconcileFromRemote(const SyncPayload& remote)
{
    // Empty gist (pre-create placeholder / truncated): never wipe local data.
    if (remote.deviceId.isEmpty() && remote.rev == 0 && remote.timestampMs == 0
        && remote.profiles.isEmpty()) {
        if (m_lastKnownRev == 0) {
            // Local has the data — push it as the real rev 1 instead of adopting emptiness.
            // Do not emit here; the debounced local push will do it.
        }
        return;
    }

    // Version rule: a strictly newer revision wins. Equal rev with empty history
    // must not be treated as newer — otherwise a fresh/empty gist would overwrite
    // the local store.
    const bool remoteNewer = remote.rev > m_lastKnownRev;
    if (!remoteNewer) {
        return;
    }

    bool applied = false;
    if (m_apply) {
        applied = m_apply(SyncPayloadCodec::toJson(remote));
    }
    m_lastKnownRev = std::max(m_lastKnownRev, remote.rev);
    SyncConfig::setLastKnownRev(m_lastKnownRev);

    if (applied) {
        emit dataUpdated();
        emit statusMessage(QStringLiteral("Newer data downloaded from sync."));
    }
}

QByteArray SyncController::serializeWithFraming()
{
    if (m_serialize) {
        const QByteArray own = m_serialize();
        SyncPayload parsed = SyncPayloadCodec::fromJson(own, nullptr);
        parsed.rev = m_lastKnownRev + 1;
        parsed.timestampMs = static_cast<qint64>(QDateTime::currentMSecsSinceEpoch());
        parsed.deviceId = m_deviceId;
        parsed.deviceLabel = m_deviceLabel;
        return SyncPayloadCodec::toJson(parsed);
    }
    SyncPayload payload;
    payload.rev = m_lastKnownRev + 1;
    payload.timestampMs = static_cast<qint64>(QDateTime::currentMSecsSinceEpoch());
    payload.deviceId = m_deviceId;
    payload.deviceLabel = m_deviceLabel;
    return SyncPayloadCodec::toJson(payload);
}
