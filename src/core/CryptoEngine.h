#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

/**
 * High-performance authenticated symmetric cryptography for the clientosh vault.
 *
 * All bulk encryption/decryption uses AES-256-GCM (via OpenSSL EVP), which
 * provides confidentiality and integrity in a single fast pass.
 *
 * Two key classes are produced here:
 *   - A fast, machine/user-bound key derived in-memory for connects.json so
 *     the connection list decrypts near-instantly at launch without any OS
 *     keyring or DPAPI latency.
 *   - A random 256-bit key for dbvault that callers persist in the OS keyring.
 */
class CryptoEngine
{
public:
    // ---- File format delimitation -------------------------------------------
    // blob layout: <salt:16><nonce:12><tag:16><ciphertext...>
    static constexpr int kSaltBytes = 16;
    static constexpr int kNonceBytes = 12;
    static constexpr int kTagBytes = 16;
    static constexpr int kKeyBytes = 32;

    // ---- Encryption API -----------------------------------------------------
    /** Encrypt plaintext under a 32-byte key. Throws CryptoError on failure. */
    static QByteArray encrypt(const QByteArray& key, const QByteArray& plaintext);

    /** Decrypt a blob (salt+nonce+tag+cipher) under a 32-byte key. Throws on failure. */
    static QByteArray decrypt(const QByteArray& key, const QByteArray& blob);

    /**
     * Return true if `blob` needs `salt` as its key-derivation input rather than
     * a raw 32-byte key. Used by callers that persist a salted file key.
     */
    static bool blobHasSalt(const QByteArray& blob);

    // ---- Key derivation ------------------------------------------------------
    /** SHA-256 digest helper (fast, single-shot). */
    static QByteArray sha256(const QByteArray& data);

    /**
     * Build a fast, machine/user-bound 32-byte key for connects.json.
     *
     * Inputs (no OS keyring/DPAPI, purely in-memory hashing):
     *   - machine-id (Linux /etc/machine-id, macOS host UUID, Windows MachineGuid)
     *   - an optional per-user scope (user SID on Windows, $USER elsewhere)
     *   - a fixed application salt / purpose string
     */
    static QByteArray deriveMachineBoundKey(const QStringList& extraScope = {});

    /** Resolve the local machine-bound material that stabilizes a per-user scope. */
    static QString machineIdentifier();

    struct CryptoError {
        QString message;
    };
};

using CryptoError = CryptoEngine::CryptoError;
