#include "SyncKey.h"
#include "SyncCrypto.h" // for size constants reuse

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace {

QString b64urlEncode(const QByteArray& data)
{
    return QString::fromLatin1(data.toBase64(QByteArray::Base64UrlEncoding
                                             | QByteArray::OmitTrailingEquals));
}

QByteArray b64urlDecode(const QString& text)
{
    return QByteArray::fromBase64Encoding(
        text.trimmed().toLatin1(),
        QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals).decoded;
}

} // namespace

namespace SyncKeyCodec {

QString encode(const SyncKey& key)
{
    if (!key.isValid()) {
        return {};
    }
    // clientosh1.<uuid>.<datakey>.<gistid>.<token>
    const QString base = QStringLiteral("clientosh1.%1.%2.%3")
        .arg(b64urlEncode(key.syncUuid),
             b64urlEncode(key.dataKey),
             key.gistId.trimmed());
    if (key.token.isEmpty()) {
        return base;
    }
    return base + QLatin1Char('.') + b64urlEncode(key.token);
}

SyncKey decode(const QString& text)
{
    SyncKey key;

    const QStringList parts = text.trimmed().split(QLatin1Char('.'));
    // Accept clientosh1.<uuid>.<datakey>.<gistid>[.<token>].
    if (parts.size() != 4 && parts.size() != 5) {
        return key;
    }
    if (parts.at(0) != QLatin1String("clientosh1")) {
        return key;
    }
    key.syncUuid = b64urlDecode(parts.at(1));
    key.dataKey = b64urlDecode(parts.at(2));
    key.gistId = parts.at(3);
    if (parts.size() == 5) {
        key.token = b64urlDecode(parts.at(4));
    }

    if (!key.isValid()) {
        return SyncKey{};
    }
    return key;
}

} // namespace SyncKeyCodec
