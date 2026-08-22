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
    mutable QMutex m_mutex;
    SessionProfile m_profile;
    QByteArray m_outgoing;
    bool m_stop = false;
    bool m_connected = false;
};
