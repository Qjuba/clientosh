#pragma once

#include "SessionProfile.h"
#include "VaultManager.h"

#include <libssh/libssh.h>

#include <QByteArray>
#include <QString>

/**
 * Loads the private key that a profile should use for public-key auth, in
 * order of preference:
 *
 *   1. A key pre-saved in the keyring (SessionProfile::privateKeyId) — imported
 *      straight from the encrypted vault in memory.
 *   2. A filesystem key path (SessionProfile::privateKeyPath).
 *
 * On success `*keyOut` is set (caller frees with ssh_key_free) and the function
 * returns true. On failure `*errorOut` (if given) describes why.
 */
inline bool loadProfilePrivateKey(const SessionProfile& profile, ssh_key* keyOut, QString* errorOut = nullptr)
{
    *keyOut = nullptr;

    const QString keyId = profile.privateKeyId.trimmed();
    if (!keyId.isEmpty()) {
        StoredKey stored;
        VaultManager vault;
        if (!vault.retrieveStoredKey(keyId, stored) || stored.pem.isEmpty()) {
            if (errorOut) {
                *errorOut = QStringLiteral("stored keyring key \"%1\" is missing").arg(keyId);
            }
            return false;
        }
        const QByteArray passphrase = profile.keyPassphrase.toUtf8();
        const char* passPtr = passphrase.isEmpty() ? nullptr : passphrase.constData();
        ssh_key privkey = nullptr;
        const int rc = ssh_pki_import_privkey_base64(stored.pem.constData(),
                                                     passPtr,
                                                     nullptr,
                                                     nullptr,
                                                     &privkey);
        // Zero the decrypted payload once imported.
        const std::size_t n = std::size_t(stored.pem.size());
        char* raw = stored.pem.data();
        for (std::size_t i = 0; i < n; ++i) {
            raw[i] = '\0';
        }
        if (rc != SSH_OK || !privkey) {
            if (errorOut) {
                *errorOut = QStringLiteral("could not decrypt stored keyring key \"%1\" (wrong passphrase or corrupted)")
                                .arg(stored.name.isEmpty() ? keyId : stored.name);
            }
            return false;
        }
        *keyOut = privkey;
        return true;
    }

    const QString keyPath = profile.privateKeyPath.trimmed();
    if (!keyPath.isEmpty()) {
        ssh_key privkey = nullptr;
        const QByteArray path = keyPath.toUtf8();
        const QByteArray passphrase = profile.keyPassphrase.toUtf8();
        const char* passPtr = passphrase.isEmpty() ? nullptr : passphrase.constData();
        if (ssh_pki_import_privkey_file(path.constData(), passPtr, nullptr, nullptr, &privkey)
            != SSH_OK || !privkey) {
            if (errorOut) {
                *errorOut = QStringLiteral("failed to load private key \"%1\"").arg(keyPath);
            }
            return false;
        }
        *keyOut = privkey;
        return true;
    }

    if (errorOut) {
        *errorOut = QStringLiteral("no private key configured");
    }
    return false;
}
