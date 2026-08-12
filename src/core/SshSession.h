#pragma once

#include <QThread>
#include <QMutex>
#include <QByteArray>
#include <QString>

struct ssh_session_struct;
struct ssh_channel_struct;
typedef struct ssh_session_struct* ssh_session;
typedef struct ssh_channel_struct* ssh_channel;

/**
 * SSH shell session running entirely on a worker thread.
 * Public methods are safe to call from the GUI thread and never block on network I/O.
 */
class SshSession : public QThread
{
    Q_OBJECT

public:
    explicit SshSession(QObject* parent = nullptr);
    ~SshSession() override;

    void connectTo(const QString& host,
                   int port,
                   const QString& user,
                   const QString& password,
                   const QString& privateKeyPath = {},
                   const QString& privateKeyId = {},
                   const QString& keyPassphrase = {});
    void disconnectFromHost();
    void sendData(const QByteArray& data);
    void resizePty(int cols, int rows);
    bool isConnected() const;

signals:
    void connected();
    void disconnected();
    void dataReceived(const QByteArray& data);
    void errorOccurred(const QString& message);
    void statusChanged(const QString& status);

protected:
    void run() override;

private:
    void cleanup();
    void abortTransport();
    bool stopRequested() const;
    bool authenticate(const QString& password,
                      const QString& privateKeyPath,
                      const QString& privateKeyId,
                      const QString& keyPassphrase,
                      QString* errorOut);

    mutable QMutex m_mutex;
    QByteArray m_pendingWrite;
    QString m_host;
    QString m_user;
    QString m_password;
    QString m_privateKeyPath;
    QString m_privateKeyId;
    QString m_keyPassphrase;
    int m_port = 22;
    int m_cols = 80;
    int m_rows = 24;
    bool m_resizePending = false;
    bool m_stopRequested = false;
    bool m_restartRequested = false;
    bool m_connected = false;

    ssh_session m_session = nullptr;
    ssh_channel m_channel = nullptr;
};
