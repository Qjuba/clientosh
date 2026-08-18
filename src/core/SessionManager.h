#pragma once

#include "SessionProfile.h"
#include "SshSession.h"

#include <QObject>
#include <QHash>
#include <QStringList>
#include <QVector>

/**
 * Owns live SSH sessions (no widgets). GUI listens to signals and owns terminals.
 */
class SessionManager : public QObject
{
    Q_OBJECT

public:
    struct LiveSession {
        QString id;
        SessionProfile profile;
        SshSession* ssh = nullptr;
        QString status = QStringLiteral("idle");
        bool connected = false;
    };

    explicit SessionManager(QObject* parent = nullptr);
    ~SessionManager() override;

    QString openSession(const SessionProfile& profile, int cols = 80, int rows = 24);
    /** Create session bookkeeping without starting SSH (UI can layout first). */
    QString createSession(const SessionProfile& profile);
    /** Begin SSH connect using the given PTY size. */
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
    void wireSession(LiveSession* live);
    void retireSsh(SshSession* ssh);

    QHash<QString, LiveSession*> m_sessions;
    QHash<QString, bool> m_detached;
    QStringList m_order;
    QString m_activeId;
    QVector<SshSession*> m_retiring;
};
