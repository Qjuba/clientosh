#include "SyncCrypto.h"

#include "CryptoEngine.h"

#include <QRandomGenerator>
#include <QUuid>

SyncKey SyncCrypto::generateSyncKey()
{
    SyncKey k;
    k.syncUuid.resize(16);
    for (int i = 0; i < 16; ++i) {
        k.syncUuid[i] = char(QRandomGenerator::global()->bounded(256));
    }
    k.dataKey.resize(CryptoEngine::kKeyBytes);
    for (int i = 0; i < CryptoEngine::kKeyBytes; ++i) {
        k.dataKey[i] = char(QRandomGenerator::global()->bounded(256));
    }

    // The actual gist id is assigned by GitHub once the gist is created; the
    // caller sets it on the returned key before sharing it.
    k.gistId.clear();

    return k;
}

QByteArray SyncCrypto::encryptPayload(const SyncKey& key, const QByteArray& plain)
{
    return CryptoEngine::encrypt(key.dataKey, plain);
}

QByteArray SyncCrypto::decryptPayload(const SyncKey& key, const QByteArray& cipher)
{
    return CryptoEngine::decrypt(key.dataKey, cipher);
}
