#pragma once

#include "CryptoEngine.h"

#include <QByteArray>
#include <QString>

/**
 * Minimal OS-keyring abstraction used by the vault.
 *
 * Each platform maps to its native password store:
 *   - Windows  : Windows Credential Manager (CredWriteW / CredReadW).
 *   - macOS    : macOS Keychain (SecItemAdd / SecItemCopyMatching).
 *   - Linux    : Secret Service via libsecret, loaded at runtime with dlopen so
 *                the build has no hard dependency on the Secret Service headers.
 *
 * If the native store is unavailable (no keyring daemon, headless/CI, etc.) the
 * adapter transparently falls back to a local file store whose entries are each
 * encrypted under a machine-bound key. This keeps dbvault working passwordlessly
 * everywhere while still protecting secrets at rest outside the OS keyring.
 *
 * All operations are synchronous and intentionally low-latency.
 */
class KeyringAdapter
{
public:
    /** Reserve a stable namespace prefix for our credentials. */
    static QString servicePrefix();

    /** true if the native OS keyring appears usable right now. */
    static bool isNativeAvailable();

    /** Store (create or overwrite) the given bytes under `key`. */
    static bool store(const QString& key, const QByteArray& data);

    /** Retrieve bytes under `key`. Returns false if absent/unreadable. */
    static bool retrieve(const QString& key, QByteArray& out);

    /** Remove the entry under `key`. Returns true if it no longer exists. */
    static bool remove(const QString& key);

    /** Where the file-backed fallback store lives (inside the vault dir). */
    static QString fallbackDir();

    // ---- internal helpers exposed for testing/diagnostics -------------------
    static bool storeNative(const QString& fullName, const QByteArray& data);
    static bool retrieveNative(const QString& fullName, QByteArray& out);
    static bool removeNative(const QString& fullName);

    static bool storeFallback(const QString& fullName, const QByteArray& data);
    static bool retrieveFallback(const QString& fullName, QByteArray& out);
    static bool removeFallback(const QString& fullName);
};
