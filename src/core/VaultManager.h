#pragma once

#include "CryptoEngine.h"
#include "KeyringAdapter.h"

#include <QJsonObject>
#include <QString>

/**
 * Coordinates the on-disk vault:
 *
 *   File 1  connects.json — connection metadata (hosts/ports/user/names).
 *       Encrypted with a fast, machine-bound key derived in-memory
 *       (SHA-256 of machine-id + per-user scope). Decrypts near-instantly at
 *       launch with no OS keyring / DPAPI / keyring latency.
 *
 *   File 2  dbvault — sensitive material (passwords, passphrases).
 *       The whole file is AES-256-GCM encrypted; its 256-bit master key lives
 *       in the OS keyring (Credential Manager / Keychain / Secret Service),
 *       retrieved transparently at launch with graceful fallback.
 *
 * Both files are written atomically (temp file + rename) so a crash or power
 * loss cannot corrupt them. Decrypted payloads are returned to callers and the
 * dbvault master key is zeroed on destruction.
 */
class VaultManager
{
public:
    enum class LoadOutcome {
        Loaded,
        NotFound, // first run / nothing saved yet
        Corrupt   // present but could not be read/decrypted
    };

    VaultManager();
    ~VaultManager();

    // ---- Paths --------------------------------------------------------------
    static QString vaultDir();
    static QString connectsPath();
    static QString dbvaultPath();

    // ---- connects.json (fast metadata) --------------------------------------
    /** Decrypt connects.json into `plainOut`. */
    LoadOutcome loadConnectsJson(QByteArray& plainOut);
    /** Encrypt and atomically write `plain` to connects.json. */
    bool saveConnectsJson(const QByteArray& plain);
    bool connectsExists() const;

    // ---- dbvault (keyring-protected secrets) --------------------------------
    /**
     * Store a per-profile secret (base64) in the encrypted dbvault file.
     * Returns false on I/O or cipher failure.
     */
    bool storeSecret(const QString& profileId, const QString& field, const QByteArray& secret);
    /** Retrieve a per-profile secret. Returns false when absent/unreadable. */
    bool retrieveSecret(const QString& profileId, const QString& field, QByteArray& secretOut);
    /** Remove a per-profile secret; best-effort. */
    bool removeSecret(const QString& profileId, const QString& field);

    // ---- diagnostics ---------------------------------------------------------
    /** true if the OS keyring path is active (vs the file-backed fallback). */
    bool usingNativeKeyring() const { return m_usingNative; }

private:
    bool ensureDbKey();
    const QByteArray& dbKey() const { return m_masterKey; }
    bool readDbvault();
    bool persistDbvault();

    QByteArray m_masterKey; // 32-byte dbvault master key (from keyring/fallback)
    QJsonObject m_dbvault;  // decoded secret map from dbvault
    bool m_dbLoaded = false;
    bool m_dbDirty = false;
    bool m_usingNative = false;
};
