#pragma once

#include <QByteArray>
#include <QString>

/**
 * Minimal GitHub Gist client used purely as an encrypted storage/transport
 * layer by the clientosh synchronization feature.
 *
 * This object is intended to be used from a worker thread (never the GUI
 * thread): each method blocks while it performs the HTTP round-trip, which is
 * acceptable off the UI thread and keeps the main thread fully responsive.
 *
 * Security contract respected here:
 *   - The API token is only ever sent as the HTTP Authorization header. It is
 *     never written into the gist, the sync payload, or returned by these calls.
 *   - Gist bodies are opaque ciphertext created by the sync layer; this client
 *     makes no attempt to interpret them.
 */
class SyncGitHub
{
public:
    struct CreateResult {
        bool ok = false;
        QString gistId;
        QString error;
    };

    struct WriteResult {
        bool ok = false;
        int serverRev = -1; // revision offered / parsed from server (unused fallback)
        QString error;
    };

    struct ReadResult {
        bool ok = false;    // true when the gist existed and body was fetched
        bool notFound = false;
        QString body;       // raw (encrypted) content of the sync file
        QString error;
    };

    /** Create a private gist with a single file holding `body`. */
    static CreateResult createGist(const QString& token, const QString& description,
                                   const QString& filename, const QByteArray& body);

    /** Verify a token by querying the authenticated user (GET /user). */
    static bool checkToken(const QString& token, QString* errorOut);

    /** Replace the sync file contents within an existing gist. */
    static WriteResult updateGist(const QString& token, const QString& gistId,
                                  const QString& filename, const QByteArray& body);

    /** Fetch the sync file contents from an existing gist. */
    static ReadResult readGist(const QString& token, const QString& gistId,
                               const QString& filename);

private:
    static bool perform(const QByteArray& method, const QString& url,
                        const QString& token, const QByteArray& payload,
                        int* httpStatusOut, QByteArray* responseBodyOut,
                        QByteArray& errorOut);
};
