#pragma once

#include <QByteArray>
#include <QString>

class QIODevice;

/**
 * Stateful XMODEM sender. The owner supplies a readable, seekable device and
 * moves bytes between this engine and the serial transport.
 */
class XmodemSender
{
public:
    enum class State {
        Idle,
        WaitingForReceiver,
        WaitingForBlockAck,
        WaitingForEotAck,
        Finished,
        Failed,
        Cancelled,
    };

    static constexpr int BlockSize = 128;
    static constexpr qint64 TimeoutMs = 10000;
    static constexpr int MaxRetries = 10;

    bool start(QIODevice* source, qint64 nowMs, QString* error = nullptr);
    /** Consume protocol bytes and return bytes received after the transfer ended. */
    QByteArray processIncoming(const QByteArray& data, qint64 nowMs);
    void checkTimeout(qint64 nowMs);
    void cancel();

    QByteArray takeOutgoing();
    State state() const { return m_state; }
    bool isActive() const;
    bool isDone() const;
    bool usesCrc() const { return m_useCrc; }
    qint64 bytesAcknowledged() const { return m_bytesAcknowledged; }
    qint64 totalBytes() const { return m_totalBytes; }
    int retries() const { return m_totalRetries; }
    QString errorString() const { return m_error; }

    static quint16 crc16(const QByteArray& data);

private:
    void sendNextBlock(qint64 nowMs);
    void retransmit(qint64 nowMs);
    void sendEot(qint64 nowMs);
    void fail(const QString& message, bool notifyPeer = true);

    QIODevice* m_source = nullptr;
    State m_state = State::Idle;
    QByteArray m_outgoing;
    QByteArray m_currentPacket;
    qint64 m_totalBytes = 0;
    qint64 m_bytesAcknowledged = 0;
    qint64 m_lastActionMs = 0;
    int m_currentPayloadSize = 0;
    int m_blockNumber = 1;
    int m_attempts = 0;
    int m_totalRetries = 0;
    int m_remoteCancelCount = 0;
    bool m_useCrc = false;
    QString m_error;
};
