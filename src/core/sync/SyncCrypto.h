#pragma once

#include "SyncKey.h"

#include <QByteArray>

/**
 * Local cryptographic helpers for the clientosh synchronization layer.
 *
 * All bulk data is protected with AES-256-GCM (through the existing
 * CryptoEngine). A fresh random 16-byte UUID identifies the sync and a random
 * 32-byte data key encrypts every payload. The data key is never persisted
 * anywhere except inside the user-held sync key, so it can never be uploaded to
 * the gist alongside the ciphertext.
 */
namespace SyncCrypto {

/** Generate a brand-new, ready-to-share synchronization key. */
SyncKey generateSyncKey();

/** Encrypt `plain` under the sync's data key. Throws CryptoEngine::CryptoError. */
QByteArray encryptPayload(const SyncKey& key, const QByteArray& plain);

/** Decrypt `cipher` under the sync's data key. Throws CryptoEngine::CryptoError. */
QByteArray decryptPayload(const SyncKey& key, const QByteArray& cipher);

} // namespace SyncCrypto
