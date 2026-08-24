#include "SessionManager.h"

#include <QUuid>

SessionManager::SessionManager(QObject* parent)
    : QObject(parent)
{
}

SessionManager::~SessionManager()
{
    const QStringList ids = m_order;
    for (const QString& id : ids) {
        closeSession(id);
    }

    // App is shutting down: wait for worker threads so QThread isn't destroyed mid-run.
    const QVector<SshSession*> retiringSsh = m_retiringSsh;
    m_retiringSsh.clear();
    for (SshSession* ssh : retiringSsh) {
        if (!ssh) {
            continue;
        }
        ssh->disconnectFromHost();
        ssh->wait(8000);
        delete ssh;
    }

    const QVector<TelnetSession*> retiringTelnet = m_retiringTelnet;
    m_retiringTelnet.clear();
    for (TelnetSession* telnet : retiringTelnet) {
        if (!telnet) {
            continue;
        }
        telnet->disconnectFromHost();
        telnet->wait(8000);
        delete telnet;
    }
    const QVector<SerialSession*> retiringSerial = m_retiringSerial;
    m_retiringSerial.clear();
    for (SerialSession* serial : retiringSerial) {
        if (!serial) continue;
        serial->disconnectFromHost();
        serial->wait(3000);
        delete serial;
    }
}

QString SessionManager::connectedLabel(const SessionProfile& profile)
{
    if (profile.isSftpOnly()) {
        return QStringLiteral("connected · SFTP");
    }
    if (profile.isTelnet()) {
        return QStringLiteral("connected · Telnet");
    }
    if (profile.isSerial()) {
        return QStringLiteral("connected · Serial");
    }
    return QStringLiteral("connected · SSH");
}

QString SessionManager::connectingLabel(const SessionProfile& profile)
{
    if (profile.isSftpOnly()) {
        return QStringLiteral("connecting sftp…");
    }
    if (profile.isTelnet()) {
        return QStringLiteral("connecting telnet…");
    }
    if (profile.isSerial()) {
        return QStringLiteral("opening serial port…");
    }
    return QStringLiteral("connecting…");
}

SessionManager::LiveSession* SessionManager::session(const QString& id)
{
    return m_sessions.value(id, nullptr);
}

const SessionManager::LiveSession* SessionManager::session(const QString& id) const
{
    return m_sessions.value(id, nullptr);
}

QStringList SessionManager::sessionIds() const
{
    return m_order;
}

QString SessionManager::createSession(const SessionProfile& profile)
{
    auto* live = new LiveSession;
    live->id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    live->profile = profile;
    live->status = QStringLiteral("preparing…");

    if (profile.isSerial()) {
        live->serial = new SerialSession(this);
        wireSerialSession(live);
    } else if (profile.isTelnet()) {
        live->telnet = new TelnetSession(this);
        wireTelnetSession(live);
    } else {
        live->ssh = new SshSession(this);
        wireSshSession(live);
    }

    m_sessions.insert(live->id, live);
    m_order.push_back(live->id);
    m_activeId = live->id;

    emit sessionOpened(live->id);
    emit sessionActivated(live->id);
    emit sessionStatusChanged(live->id, live->status);

    return live->id;
}

void SessionManager::connectSession(const QString& id, int cols, int rows)
{
    LiveSession* live = session(id);
    if (!live) {
        return;
    }

    live->status = QStringLiteral("connecting...");
    emit sessionStatusChanged(id, live->status);

    if (live->serial) {
        live->serial->connectTo(live->profile);
        return;
    }

    if (live->telnet) {
        live->telnet->resizePty(cols, rows);
        live->telnet->connectTo(live->profile.host,
                                live->profile.port,
                                live->profile.user,
                                live->profile.password);
        return;
    }

    if (!live->ssh) {
        return;
    }

    live->ssh->resizePty(cols, rows);
    live->ssh->connectTo(live->profile.host,
                         live->profile.port,
                         live->profile.user,
                         live->profile.password,
                         live->profile.privateKeyPath,
                         live->profile.privateKeyId,
                         live->profile.keyPassphrase,
                         live->profile.authMethod);
}

QString SessionManager::openSession(const SessionProfile& profile, int cols, int rows)
{
    const QString id = createSession(profile);
    connectSession(id, cols, rows);
    return id;
}

void SessionManager::wireSshSession(LiveSession* live)
{
    const QString id = live->id;

    connect(live->ssh, &SshSession::connected, this, [this, id]() {
        if (auto* s = session(id)) {
            s->connected = true;
            s->status = connectedLabel(s->profile);
            emit sessionConnectionChanged(id, true);
            emit sessionStatusChanged(id, s->status);
        }
    });

    connect(live->ssh, &SshSession::systemDetected, this, [this, id](const QString& system) {
        if (auto* s = session(id)) {
            s->profile.system = system;
        }
        emit sessionSystemDetected(id, system);
    });

    connect(live->ssh, &SshSession::disconnected, this, [this, id]() {
        if (auto* s = session(id)) {
            s->connected = false;
            s->status = QStringLiteral("disconnected");
            emit sessionConnectionChanged(id, false);
            emit sessionStatusChanged(id, s->status);
            emit sessionDataReceived(id, QByteArray("\r\n[session closed]\r\n"));
        }
    });

    connect(live->ssh, &SshSession::dataReceived, this, [this, id](const QByteArray& data) {
        if (session(id)) {
            emit sessionDataReceived(id, data);
        }
    });

    connect(live->ssh, &SshSession::errorOccurred, this, [this, id](const QString& message) {
        if (auto* s = session(id)) {
            s->connected = false;
            s->status = message;
            emit sessionConnectionChanged(id, false);
            emit sessionStatusChanged(id, s->status);
            emit sessionError(id, message);
            emit sessionDataReceived(
                id,
                (QStringLiteral("\r\n[error] ") + message + QStringLiteral("\r\n")).toUtf8());
        }
    });

    connect(live->ssh, &SshSession::statusChanged, this, [this, id](const QString& status) {
        if (auto* s = session(id)) {
            QString sanitized = status;
            if (status.startsWith(QLatin1String("connected "))) {
                sanitized = connectedLabel(s->profile);
            } else if (status.startsWith(QLatin1String("connecting to "))) {
                sanitized = connectingLabel(s->profile);
            }
            s->status = sanitized;
            emit sessionStatusChanged(id, sanitized);
        }
    });
}

void SessionManager::wireTelnetSession(LiveSession* live)
{
    const QString id = live->id;

    connect(live->telnet, &TelnetSession::connected, this, [this, id]() {
        if (auto* s = session(id)) {
            s->connected = true;
            s->status = connectedLabel(s->profile);
            emit sessionConnectionChanged(id, true);
            emit sessionStatusChanged(id, s->status);
        }
    });

    connect(live->telnet, &TelnetSession::disconnected, this, [this, id]() {
        if (auto* s = session(id)) {
            s->connected = false;
            s->status = QStringLiteral("disconnected");
            emit sessionConnectionChanged(id, false);
            emit sessionStatusChanged(id, s->status);
            emit sessionDataReceived(id, QByteArray("\r\n[session closed]\r\n"));
        }
    });

    connect(live->telnet, &TelnetSession::dataReceived, this, [this, id](const QByteArray& data) {
        if (session(id)) {
            emit sessionDataReceived(id, data);
        }
    });

    connect(live->telnet, &TelnetSession::errorOccurred, this, [this, id](const QString& message) {
        if (auto* s = session(id)) {
            s->connected = false;
            s->status = message;
            emit sessionConnectionChanged(id, false);
            emit sessionStatusChanged(id, s->status);
            emit sessionError(id, message);
            emit sessionDataReceived(
                id,
                (QStringLiteral("\r\n[error] ") + message + QStringLiteral("\r\n")).toUtf8());
        }
    });

    connect(live->telnet, &TelnetSession::statusChanged, this, [this, id](const QString& status) {
        if (auto* s = session(id)) {
            QString sanitized = status;
            if (status.startsWith(QLatin1String("connected "))) {
                sanitized = connectedLabel(s->profile);
            } else if (status.startsWith(QLatin1String("connecting to "))) {
                sanitized = connectingLabel(s->profile);
            }
            s->status = sanitized;
            emit sessionStatusChanged(id, sanitized);
        }
    });
}

void SessionManager::wireSerialSession(LiveSession* live)
{
    const QString id = live->id;
    connect(live->serial, &SerialSession::connected, this, [this, id]() {
        if (auto* s = session(id)) {
            s->connected = true;
            s->status = connectedLabel(s->profile);
            emit sessionConnectionChanged(id, true);
            emit sessionStatusChanged(id, s->status);
        }
    });
    connect(live->serial, &SerialSession::disconnected, this, [this, id]() {
        if (auto* s = session(id)) {
            s->connected = false;
            s->status = QStringLiteral("disconnected");
            emit sessionConnectionChanged(id, false);
            emit sessionStatusChanged(id, s->status);
            emit sessionDataReceived(id, QByteArray("\r\n[serial port closed]\r\n"));
        }
    });
    connect(live->serial, &SerialSession::dataReceived, this, [this, id](const QByteArray& data) {
        if (session(id)) emit sessionDataReceived(id, data);
    });
    connect(live->serial, &SerialSession::errorOccurred, this, [this, id](const QString& message) {
        if (auto* s = session(id)) {
            s->connected = false;
            s->status = message;
            emit sessionConnectionChanged(id, false);
            emit sessionStatusChanged(id, message);
            emit sessionError(id, message);
            emit sessionDataReceived(id, (QStringLiteral("\r\n[error] ") + message
                                          + QStringLiteral("\r\n")).toUtf8());
        }
    });
    connect(live->serial, &SerialSession::statusChanged, this, [this, id](const QString& status) {
        if (auto* s = session(id)) {
            s->status = status.startsWith(QLatin1String("connected "))
                ? connectedLabel(s->profile) : status;
            emit sessionStatusChanged(id, s->status);
        }
    });
    connect(live->serial, &SerialSession::xmodemStarted, this,
            [this, id](qint64 totalBytes) {
                if (session(id)) emit xmodemStarted(id, totalBytes);
            });
    connect(live->serial, &SerialSession::xmodemProgress, this,
            [this, id](qint64 sentBytes, qint64 totalBytes, int retries) {
                if (session(id)) emit xmodemProgress(id, sentBytes, totalBytes, retries);
            });
    connect(live->serial, &SerialSession::xmodemFinished, this, [this, id]() {
        if (session(id)) emit xmodemFinished(id);
    });
    connect(live->serial, &SerialSession::xmodemError, this,
            [this, id](const QString& message) {
                if (session(id)) emit xmodemError(id, message);
            });
}

void SessionManager::retireSsh(SshSession* ssh)
{
    if (!ssh) {
        return;
    }

    QObject::disconnect(ssh, nullptr, this, nullptr);
    ssh->setParent(nullptr);

    if (!ssh->isRunning()) {
        ssh->deleteLater();
        return;
    }

    m_retiringSsh.push_back(ssh);
    connect(ssh, &QThread::finished, this, [this, ssh]() {
        m_retiringSsh.removeAll(ssh);
        ssh->deleteLater();
    });
    ssh->disconnectFromHost();
}

void SessionManager::retireTelnet(TelnetSession* telnet)
{
    if (!telnet) {
        return;
    }

    QObject::disconnect(telnet, nullptr, this, nullptr);
    telnet->setParent(nullptr);

    if (!telnet->isRunning()) {
        telnet->deleteLater();
        return;
    }

    m_retiringTelnet.push_back(telnet);
    connect(telnet, &QThread::finished, this, [this, telnet]() {
        m_retiringTelnet.removeAll(telnet);
        telnet->deleteLater();
    });
    telnet->disconnectFromHost();
}

void SessionManager::retireSerial(SerialSession* serial)
{
    if (!serial) return;
    QObject::disconnect(serial, nullptr, this, nullptr);
    serial->setParent(nullptr);
    if (!serial->isRunning()) {
        serial->deleteLater();
        return;
    }
    m_retiringSerial.push_back(serial);
    connect(serial, &QThread::finished, this, [this, serial]() {
        m_retiringSerial.removeAll(serial);
        serial->deleteLater();
    });
    serial->disconnectFromHost();
}

void SessionManager::closeSession(const QString& id)
{
    LiveSession* live = m_sessions.take(id);
    if (!live) {
        return;
    }
    m_order.removeAll(id);
    m_detached.remove(id);

    retireSsh(live->ssh);
    retireTelnet(live->telnet);
    retireSerial(live->serial);
    live->ssh = nullptr;
    live->telnet = nullptr;
    live->serial = nullptr;
    delete live;

    if (m_activeId == id) {
        m_activeId = m_order.isEmpty() ? QString() : m_order.last();
        if (!m_activeId.isEmpty()) {
            emit sessionActivated(m_activeId);
        }
    }

    emit sessionClosed(id);
}

void SessionManager::disconnectSession(const QString& id)
{
    if (auto* s = session(id); s) {
        if (s->ssh) {
            s->ssh->disconnectFromHost();
        }
        if (s->telnet) {
            s->telnet->disconnectFromHost();
        }
        if (s->serial) {
            s->serial->disconnectFromHost();
        }
    }
}

void SessionManager::sendData(const QString& id, const QByteArray& data)
{
    if (auto* s = session(id); s) {
        if (s->ssh) {
            s->ssh->sendData(data);
        } else if (s->telnet) {
            s->telnet->sendData(data);
        } else if (s->serial) {
            s->serial->sendData(data);
        }
    }
}

void SessionManager::startXmodem(const QString& id, const QString& filePath)
{
    if (auto* s = session(id); s && s->serial) s->serial->startXmodem(filePath);
    else emit xmodemError(id, QStringLiteral("XMODEM is available only for serial sessions"));
}

void SessionManager::cancelXmodem(const QString& id)
{
    if (auto* s = session(id); s && s->serial) s->serial->cancelXmodem();
}

void SessionManager::resizePty(const QString& id, int cols, int rows)
{
    if (auto* s = session(id); s) {
        if (s->ssh) {
            s->ssh->resizePty(cols, rows);
        } else if (s->telnet) {
            s->telnet->resizePty(cols, rows);
        }
    }
}

void SessionManager::activateSession(const QString& id)
{
    if (!m_sessions.contains(id) || m_activeId == id) {
        return;
    }
    m_activeId = id;
    emit sessionActivated(id);
}

void SessionManager::setDetached(const QString& id, bool detached)
{
    if (!m_sessions.contains(id)) {
        return;
    }
    m_detached[id] = detached;
    emit sessionDetachChanged(id, detached);
}

bool SessionManager::isDetached(const QString& id) const
{
    return m_detached.value(id, false);
}

QStringList SessionManager::attachedSessionIds() const
{
    QStringList out;
    for (const QString& id : m_order) {
        if (!isDetached(id)) {
            out.push_back(id);
        }
    }
    return out;
}
