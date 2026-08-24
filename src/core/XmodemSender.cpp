#include "XmodemSender.h"

#include <QIODevice>

namespace {
constexpr char Soh = 0x01;
constexpr char Eot = 0x04;
constexpr char Ack = 0x06;
constexpr char Nak = 0x15;
constexpr char Can = 0x18;
constexpr char CrcRequest = 0x43;
constexpr char Padding = 0x1a;
}

bool XmodemSender::start(QIODevice* source, qint64 nowMs, QString* error)
{
    if (!source || !source->isOpen() || !source->isReadable() || source->isSequential()) {
        const QString message = QStringLiteral("XMODEM source must be an open, seekable file");
        if (error) *error = message;
        return false;
    }
    if (!source->seek(0)) {
        const QString message = QStringLiteral("cannot seek to the start of the XMODEM file");
        if (error) *error = message;
        return false;
    }

    m_source = source;
    m_state = State::WaitingForReceiver;
    m_outgoing.clear();
    m_currentPacket.clear();
    m_totalBytes = source->size();
    m_bytesAcknowledged = 0;
    m_lastActionMs = nowMs;
    m_currentPayloadSize = 0;
    m_blockNumber = 1;
    m_attempts = 0;
    m_totalRetries = 0;
    m_remoteCancelCount = 0;
    m_useCrc = false;
    m_error.clear();
    return true;
}

bool XmodemSender::isActive() const
{
    return m_state == State::WaitingForReceiver || m_state == State::WaitingForBlockAck
        || m_state == State::WaitingForEotAck;
}

bool XmodemSender::isDone() const
{
    return m_state == State::Finished || m_state == State::Failed || m_state == State::Cancelled;
}

quint16 XmodemSender::crc16(const QByteArray& data)
{
    quint16 crc = 0;
    for (const unsigned char byte : data) {
        crc ^= static_cast<quint16>(byte) << 8;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000) ? static_cast<quint16>((crc << 1) ^ 0x1021)
                                 : static_cast<quint16>(crc << 1);
        }
    }
    return crc;
}

QByteArray XmodemSender::processIncoming(const QByteArray& data, qint64 nowMs)
{
    for (qsizetype index = 0; index < data.size(); ++index) {
        if (!isActive()) return data.mid(index);
        const unsigned char byte = static_cast<unsigned char>(data[index]);

        if (byte == static_cast<unsigned char>(Can)) {
            if (++m_remoteCancelCount >= 2) {
                m_state = State::Cancelled;
                m_error = QStringLiteral("transfer cancelled by the receiver");
            }
            continue;
        }
        m_remoteCancelCount = 0;

        if (m_state == State::WaitingForReceiver) {
            if (byte == static_cast<unsigned char>(CrcRequest) || byte == static_cast<unsigned char>(Nak)) {
                m_useCrc = byte == static_cast<unsigned char>(CrcRequest);
                m_attempts = 0;
                sendNextBlock(nowMs);
            }
            continue;
        }

        if (m_state == State::WaitingForBlockAck) {
            if (byte == static_cast<unsigned char>(Ack)) {
                m_bytesAcknowledged += m_currentPayloadSize;
                m_currentPacket.clear();
                m_currentPayloadSize = 0;
                m_blockNumber = (m_blockNumber + 1) & 0xff;
                m_attempts = 0;
                sendNextBlock(nowMs);
            } else if (byte == static_cast<unsigned char>(Nak)) {
                retransmit(nowMs);
            }
            continue;
        }

        if (m_state == State::WaitingForEotAck) {
            if (byte == static_cast<unsigned char>(Ack)) {
                m_state = State::Finished;
            } else if (byte == static_cast<unsigned char>(Nak)) {
                sendEot(nowMs);
            }
        }
    }
    return {};
}

void XmodemSender::sendNextBlock(qint64 nowMs)
{
    const QByteArray raw = m_source->read(BlockSize);
    if (raw.isEmpty()) {
        if (m_source->atEnd()) {
            sendEot(nowMs);
        } else {
            fail(QStringLiteral("failed to read the XMODEM source file"));
        }
        return;
    }

    QByteArray payload = raw;
    m_currentPayloadSize = raw.size();
    payload.append(BlockSize - payload.size(), Padding);

    m_currentPacket.clear();
    m_currentPacket.reserve(3 + BlockSize + (m_useCrc ? 2 : 1));
    m_currentPacket.append(Soh);
    m_currentPacket.append(static_cast<char>(m_blockNumber));
    m_currentPacket.append(static_cast<char>(0xff - m_blockNumber));
    m_currentPacket.append(payload);
    if (m_useCrc) {
        const quint16 crc = crc16(payload);
        m_currentPacket.append(static_cast<char>((crc >> 8) & 0xff));
        m_currentPacket.append(static_cast<char>(crc & 0xff));
    } else {
        quint8 checksum = 0;
        for (const unsigned char byte : payload) checksum = static_cast<quint8>(checksum + byte);
        m_currentPacket.append(static_cast<char>(checksum));
    }

    m_outgoing.append(m_currentPacket);
    m_state = State::WaitingForBlockAck;
    m_lastActionMs = nowMs;
}

void XmodemSender::retransmit(qint64 nowMs)
{
    if (++m_attempts > MaxRetries) {
        fail(QStringLiteral("too many XMODEM block retries"));
        return;
    }
    ++m_totalRetries;
    m_outgoing.append(m_currentPacket);
    m_lastActionMs = nowMs;
}

void XmodemSender::sendEot(qint64 nowMs)
{
    if (m_state == State::WaitingForEotAck) {
        if (++m_attempts > MaxRetries) {
            fail(QStringLiteral("receiver did not acknowledge end of transfer"));
            return;
        }
        ++m_totalRetries;
    } else {
        m_attempts = 0;
    }
    m_outgoing.append(Eot);
    m_state = State::WaitingForEotAck;
    m_lastActionMs = nowMs;
}

void XmodemSender::checkTimeout(qint64 nowMs)
{
    if (!isActive() || nowMs - m_lastActionMs < TimeoutMs) return;

    if (m_state == State::WaitingForReceiver) {
        if (++m_attempts > MaxRetries) {
            fail(QStringLiteral("receiver did not start XMODEM"));
        } else {
            ++m_totalRetries;
            m_lastActionMs = nowMs;
        }
    } else if (m_state == State::WaitingForBlockAck) {
        retransmit(nowMs);
    } else if (m_state == State::WaitingForEotAck) {
        sendEot(nowMs);
    }
}

void XmodemSender::cancel()
{
    if (!isActive()) return;
    m_outgoing.append(QByteArray(2, Can));
    m_state = State::Cancelled;
    m_error = QStringLiteral("transfer cancelled");
}

void XmodemSender::fail(const QString& message, bool notifyPeer)
{
    if (notifyPeer) m_outgoing.append(QByteArray(2, Can));
    m_state = State::Failed;
    m_error = message;
}

QByteArray XmodemSender::takeOutgoing()
{
    QByteArray result;
    result.swap(m_outgoing);
    return result;
}
