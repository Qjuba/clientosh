#pragma once

#include "SessionProfile.h"
#include "SshSession.h"
#include "TelnetSession.h"

#include <QObject>
#include <QHash>
#include <QStringList>
#include <QVector>

/**
 * Owns live terminal sessions (no widgets). GUI listens to signals and owns terminals.
 */
class SessionManager : public QObject
{
    Q_OBJECT

public:
    struct LiveSession {
        QString id;
        SessionProfile profile;
        SshSession* ssh = nullptr;
        TelnetSession* telnet = nullptr;
        QString status = QStringLiteral("idle");
        bool connected = false;

        bool usesTelnet() const { return profile.isTelnet(); }
    };

    explicit SessionManager(QObject* parent = nullptr);
    ~SessionManager() override;

    QString openSession(const SessionProfile& profile, int cols = 80, int rows = 24);
    /** Create session bookkeeping without starting transport (UI can layout first). */
    QString createSession(const SessionProfile& profile);
    /** Begin connect using the given terminal size. */
    void connectSession(const QString& id, int cols, int rows);
    void closeSession(const QString& id);
    void disconnectSession(const QString& id);
    void activateSession(const QString& id);
    void setDetached(const QString& id, bool detached);
    bool isDetached(const QString& id) const;

    void sendData(const QString& id, const QByteArray& data);
    void resizePty(const QString& id, int cols, int rows);

    QString activeId() const { return m_activeId; }
    LiveSession* session(const QString& id);
    const LiveSession* session(const QString& id) const;
    QStringList sessionIds() const;
    QStringList attachedSessionIds() const;
    int count() const { return m_order.size(); }

signals:
    void sessionOpened(const QString& id);
    void sessionClosed(const QString& id);
    void sessionActivated(const QString& id);
    void sessionStatusChanged(const QString& id, const QString& status);
    void sessionConnectionChanged(const QString& id, bool connected);
    /** Id is the session id; carry the detected OS so the dashboard can update the profile. */
    void sessionSystemDetected(const QString& id, const QString& system);
    void sessionDetachChanged(const QString& id, bool detached);
    void sessionDataReceived(const QString& id, const QByteArray& data);
    void sessionError(const QString& id, const QString& message);

private:
    static QString connectedLabel(const SessionProfile& profile);
    static QString connectingLabel(const SessionProfile& profile);

    void wireSshSession(LiveSession* live);
    void wireTelnetSession(LiveSession* live);
    void retireSsh(SshSession* ssh);
    void retireTelnet(TelnetSession* telnet);

    QHash<QString, LiveSession*> m_sessions;
    QHash<QString, bool> m_detached;
    QStringList m_order;
    QString m_activeId;
    QVector<SshSession*> m_retiringSsh;
    QVector<TelnetSession*> m_retiringTelnet;
};
