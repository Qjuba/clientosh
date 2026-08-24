#pragma once

#include "SessionProfile.h"

#include <QByteArray>
#include <QMutex>
#include <QStringList>
#include <QThread>

/** Local serial/COM terminal transport. All blocking I/O stays on this worker thread. */
class SerialSession : public QThread
{
    Q_OBJECT

public:
    explicit SerialSession(QObject* parent = nullptr);
    ~SerialSession() override;

    static QStringList availablePorts();

    void connectTo(const SessionProfile& profile);
    void disconnectFromHost();
    void sendData(const QByteArray& data);
    void startXmodem(const QString& filePath);
    void cancelXmodem();
    bool isConnected() const;
    bool isXmodemActive() const;

signals:
    void connected();
    void disconnected();
    void dataReceived(const QByteArray& data);
    void errorOccurred(const QString& message);
    void statusChanged(const QString& status);
    void xmodemStarted(qint64 totalBytes);
    void xmodemProgress(qint64 sentBytes, qint64 totalBytes, int retries);
    void xmodemFinished();
    void xmodemError(const QString& message);

protected:
    void run() override;

private:
    mutable QMutex m_mutex;
    SessionProfile m_profile;
    QByteArray m_outgoing;
    QString m_xmodemRequestPath;
    bool m_stop = false;
    bool m_connected = false;
    bool m_xmodemActive = false;
    bool m_xmodemCancelRequested = false;
};
