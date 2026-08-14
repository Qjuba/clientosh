#pragma once

#include "SyncKey.h"

#include <QString>

/**
 * Local persistence for the synchronization feature.
 *
 * Security contract:
 *   - The GitHub API token is stored only in the OS keyring (via KeyringAdapter),
 *     keyed to this sync. It is never written to the gist or into sync data.
 *   - The portable sync key (which contains the decryption data key) is stored in
 *     QSettings. Even though QSettings is not hardened storage, this is the same
 *     trust domain as the vault's fast metadata and keeps the key local; it is
 *     never uploaded anywhere.
 *   - Nothing in this struct is ever sent over the network.
 */
namespace SyncConfig {

/** Whether sync is enabled in this installation. */
bool enabled();
void setEnabled(bool on);

/** The full portable sync key string (empty when not set). */
QString syncKeyText();
void setSyncKeyText(const QString& text);

/** Resolve the parsed sync key from QSettings (invalid if unset/parse error). */
SyncKey syncKey();

/** Human description of the created gist (informational only). */
QString gistDescription();
void setGistDescription(const QString& text);

/** Poll interval in seconds (10..3600). */
int pollIntervalSec();
void setPollIntervalSec(int seconds);

/** Last known remote revision persisted across runs (best effort). */
int lastKnownRev();
void setLastKnownRev(int rev);

// ---- Token (OS keyring only) -------------------------------------------
/** Persist the GitHub token for `uuidHex` in the OS keyring. */
void storeToken(const QString& uuidHex, const QString& token);
/** Retrieve the token; empty string when absent. */
QString loadToken(const QString& uuidHex);
/** Remove the stored token. */
void clearToken(const QString& uuidHex);

} // namespace SyncConfig
