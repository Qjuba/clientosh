#pragma once

#include <QThread>
#include <QMutex>
#include <QByteArray>
#include <QString>

class QTcpSocket;

/**
 * Telnet terminal session on a worker thread (RFC 854 + NAWS).
 * Public methods are safe from the GUI thread and never block on network I/O.
 */
class TelnetSession : public QThread
{
    Q_OBJECT

public:
    explicit TelnetSession(QObject* parent = nullptr);
    ~TelnetSession() override;

    void connectTo(const QString& host, int port, const QString& user, const QString& password);
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
    bool stopRequested() const;
    void abortTransport(QTcpSocket* socket);

    mutable QMutex m_mutex;
    QByteArray m_pendingWrite;
    QString m_host;
    QString m_user;
    QString m_password;
    int m_port = 23;
    int m_cols = 80;
    int m_rows = 24;
    bool m_resizePending = false;
    bool m_stopRequested = false;
    bool m_restartRequested = false;
    bool m_connected = false;
};
