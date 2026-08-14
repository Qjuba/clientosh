#pragma once

#include <QByteArray>
#include <QString>

/**
 * A parsed clientosh synchronization key.
 *
 * The sync key is a single portable string that carries *everything* needed to
 * join an existing synchronization, including the GitHub API token:
 *
 *   - syncUuid : 16-byte unique identifier of the synchronization.
 *   - dataKey  : 32-byte AES-256-GCM key that encrypts all payloads on the gist.
 *   - gistId   : GitHub gist identifier used only as a storage locator.
 *   - token    : GitHub API token used to authenticate against the gist.
 *
 * The token is embedded so that one key is enough to fully join the sync on any
 * machine. It is never written into the gist itself or into any synced payload —
 * it only lives inside the user-held key.
 *
 * Format:  clientosh1.<b64u(uuid16)>.<b64u(dataKey32)>.<gistId>.<b64u(token)>
 * (b64u = unpadded base64url). decode() also accepts the older 4-part form by
 * leaving the token empty.
 */
struct SyncKey
{
    QByteArray syncUuid; // 16 bytes
    QByteArray dataKey;  // 32 bytes
    QString gistId;
    QByteArray token;    // GitHub API token (may be empty for legacy keys)

    bool isValid() const
    {
        return syncUuid.size() == 16 && dataKey.size() == 32 && !gistId.trimmed().isEmpty();
    }
};

namespace SyncKeyCodec {

/** Encode a parsed key into the portable string form. Empty on invalid input. */
QString encode(const SyncKey& key);

/** Decode a sync key string. isValid()==false signals malformed input. */
SyncKey decode(const QString& text);

} // namespace SyncKeyCodec
