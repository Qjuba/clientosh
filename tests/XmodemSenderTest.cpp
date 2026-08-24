#include "core/XmodemSender.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDebug>

namespace {
bool require(bool condition, const char* message)
{
    if (!condition) qCritical() << message;
    return condition;
}

bool testCrcTransfer()
{
    if (!require(XmodemSender::crc16(QByteArray("123456789")) == 0x31c3,
                 "CRC-16/XMODEM reference vector is wrong")) return false;
    QByteArray content;
    for (int i = 0; i < 129; ++i) content.append(static_cast<char>(i));
    QBuffer source(&content);
    source.open(QIODevice::ReadOnly);
    XmodemSender sender;
    if (!require(sender.start(&source, 0), "start failed")) return false;

    sender.processIncoming(QByteArray(1, 'C'), 1);
    const QByteArray first = sender.takeOutgoing();
    if (!require(first.size() == 133, "CRC packet size is wrong")) return false;
    if (!require(static_cast<unsigned char>(first[0]) == 0x01 && first[1] == 1
                     && static_cast<unsigned char>(first[2]) == 0xfe,
                 "first packet header is wrong")) return false;
    const QByteArray firstPayload = first.mid(3, 128);
    const quint16 crc = XmodemSender::crc16(firstPayload);
    if (!require(static_cast<unsigned char>(first[131]) == (crc >> 8)
                     && static_cast<unsigned char>(first[132]) == (crc & 0xff),
                 "CRC bytes are wrong")) return false;

    sender.processIncoming(QByteArray(1, 0x06), 2);
    const QByteArray second = sender.takeOutgoing();
    if (!require(second.size() == 133 && second[1] == 2, "second packet is wrong")) return false;
    sender.processIncoming(QByteArray(1, 0x06), 3);
    if (!require(sender.takeOutgoing() == QByteArray(1, 0x04), "EOT was not sent")) return false;
    QByteArray finalReply(1, 0x06);
    finalReply.append("booting");
    const QByteArray remainder = sender.processIncoming(finalReply, 4);
    return require(sender.state() == XmodemSender::State::Finished
                       && sender.bytesAcknowledged() == content.size()
                       && remainder == QByteArray("booting"),
                   "transfer did not finish");
}

bool testChecksumAndRetry()
{
    QByteArray content("abc");
    QBuffer source(&content);
    source.open(QIODevice::ReadOnly);
    XmodemSender sender;
    if (!sender.start(&source, 0)) return false;
    sender.processIncoming(QByteArray(1, 0x15), 1);
    const QByteArray packet = sender.takeOutgoing();
    if (!require(packet.size() == 132 && !sender.usesCrc(), "checksum packet is wrong")) return false;
    quint8 expectedChecksum = 0;
    for (const unsigned char byte : packet.mid(3, 128)) {
        expectedChecksum = static_cast<quint8>(expectedChecksum + byte);
    }
    if (!require(static_cast<unsigned char>(packet.back()) == expectedChecksum,
                 "8-bit checksum is wrong")) return false;
    sender.processIncoming(QByteArray(1, 0x15), 2);
    return require(sender.takeOutgoing() == packet && sender.retries() == 1,
                   "NAK did not retransmit the packet");
}

bool testEmptyFileAndLocalCancel()
{
    QByteArray content;
    QBuffer source(&content);
    source.open(QIODevice::ReadOnly);
    XmodemSender sender;
    if (!sender.start(&source, 0)) return false;
    sender.processIncoming(QByteArray(1, 'C'), 1);
    if (!require(sender.takeOutgoing() == QByteArray(1, 0x04),
                 "empty file did not go directly to EOT")) return false;

    source.seek(0);
    if (!sender.start(&source, 0)) return false;
    sender.cancel();
    return require(sender.state() == XmodemSender::State::Cancelled
                       && sender.takeOutgoing() == QByteArray(2, 0x18),
                   "local cancel did not send two CAN bytes");
}

bool testCancelAndTimeout()
{
    QByteArray content("abc");
    QBuffer source(&content);
    source.open(QIODevice::ReadOnly);
    XmodemSender sender;
    if (!sender.start(&source, 0)) return false;
    for (int i = 1; i <= XmodemSender::MaxRetries + 1; ++i) {
        sender.checkTimeout(i * XmodemSender::TimeoutMs);
    }
    if (!require(sender.state() == XmodemSender::State::Failed, "startup timeout did not fail")) return false;

    source.seek(0);
    if (!sender.start(&source, 0)) return false;
    sender.processIncoming(QByteArray(1, 0x18), 1);
    if (!require(sender.isActive(), "a single CAN cancelled the transfer")) return false;
    sender.processIncoming(QByteArray(1, 0x18), 2);
    return require(sender.state() == XmodemSender::State::Cancelled,
                   "remote CAN did not cancel transfer");
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    return testCrcTransfer() && testChecksumAndRetry() && testEmptyFileAndLocalCancel()
            && testCancelAndTimeout() ? 0 : 1;
}
