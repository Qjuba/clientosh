#pragma once

#include "VaultManager.h"

#include <QString>
#include <QVector>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QUuid>
#include <QMetaType>

enum class ConnectionMode {
    Ssh = 0,     // interactive terminal + optional SFTP
    SftpOnly = 1 // open SFTP file manager only
};

struct SessionProfile {
    QString id;
    QString name;
    QString host;
    int port = 22;
    QString user;
    QString password;
    bool savePassword = false;
    QString privateKeyPath; // if set, prefer public-key auth (filesystem path)
    QString privateKeyId;   // if set, use a key pre-saved in the keyring (overrides path)
    QString keyPassphrase;  // for encrypted keys (not persisted by default)
    bool saveKeyPassphrase = false;
    ConnectionMode connectionMode = ConnectionMode::Ssh;

    bool usesPrivateKey() const
    {
        return !privateKeyId.trimmed().isEmpty() || !privateKeyPath.trimmed().isEmpty();
    }

    bool isSftpOnly() const
    {
        return connectionMode == ConnectionMode::SftpOnly;
    }

    QString connectionTypeLabel() const
    {
        return isSftpOnly() ? QStringLiteral("SFTP") : QStringLiteral("SSH");
    }

    /** Display name without exposing host/IP on the dashboard. */
    QString displayTitle() const
    {
        if (!name.trimmed().isEmpty()) {
            return name.trimmed();
        }
        if (!user.isEmpty()) {
            return user;
        }
        return QStringLiteral("session");
    }

    QString endpoint() const
    {
        return QStringLiteral("%1:%2").arg(host).arg(port);
    }
};

Q_DECLARE_METATYPE(SessionProfile)

inline ConnectionMode connectionModeFromString(const QString& value)
{
    if (value.compare(QLatin1String("sftp"), Qt::CaseInsensitive) == 0
        || value.compare(QLatin1String("sftpOnly"), Qt::CaseInsensitive) == 0) {
        return ConnectionMode::SftpOnly;
    }
    return ConnectionMode::Ssh;
}

inline QString connectionModeToString(ConnectionMode mode)
{
    return mode == ConnectionMode::SftpOnly ? QStringLiteral("sftp") : QStringLiteral("ssh");
}

// ---- Legacy QSettings persistence (migration + graceful fallback) -------
inline QVector<SessionProfile> profilesFromQSettings()
{
    QSettings s;
    QVector<SessionProfile> out;
    const int n = s.beginReadArray(QStringLiteral("profiles"));
    for (int i = 0; i < n; ++i) {
        s.setArrayIndex(i);
        SessionProfile p;
        p.id = s.value(QStringLiteral("id")).toString();
        p.name = s.value(QStringLiteral("name")).toString();
        p.host = s.value(QStringLiteral("host")).toString();
        p.port = s.value(QStringLiteral("port"), 22).toInt();
        p.user = s.value(QStringLiteral("user")).toString();
        p.savePassword = s.value(QStringLiteral("savePassword"), false).toBool();
        if (p.savePassword) {
            p.password = s.value(QStringLiteral("password")).toString();
        }
        p.privateKeyPath = s.value(QStringLiteral("privateKeyPath")).toString();
        p.privateKeyId = s.value(QStringLiteral("privateKeyId")).toString();
        p.saveKeyPassphrase = s.value(QStringLiteral("saveKeyPassphrase"), false).toBool();
        if (p.saveKeyPassphrase) {
            p.keyPassphrase = s.value(QStringLiteral("keyPassphrase")).toString();
        }
        p.connectionMode = connectionModeFromString(
            s.value(QStringLiteral("connectionMode"), QStringLiteral("ssh")).toString());
        if (p.id.isEmpty()) {
            p.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
        out.push_back(p);
    }
    s.endArray();
    return out;
}

// ---- Vault (encrypted connects.json + keyring dbvault) --------------------
namespace VaultPrivate {

inline QJsonObject profileToJson(const SessionProfile& p)
{
    QJsonObject o;
    o.insert(QStringLiteral("id"), p.id);
    o.insert(QStringLiteral("name"), p.name);
    o.insert(QStringLiteral("host"), p.host);
    o.insert(QStringLiteral("port"), p.port);
    o.insert(QStringLiteral("user"), p.user);
    o.insert(QStringLiteral("savePassword"), p.savePassword);
    o.insert(QStringLiteral("privateKeyPath"), p.privateKeyPath);
    o.insert(QStringLiteral("privateKeyId"), p.privateKeyId);
    o.insert(QStringLiteral("saveKeyPassphrase"), p.saveKeyPassphrase);
    o.insert(QStringLiteral("connectionMode"), connectionModeToString(p.connectionMode));
    return o;
}

inline SessionProfile profileFromJson(const QJsonObject& o)
{
    SessionProfile p;
    p.id = o.value(QStringLiteral("id")).toString();
    p.name = o.value(QStringLiteral("name")).toString();
    p.host = o.value(QStringLiteral("host")).toString();
    p.port = o.value(QStringLiteral("port")).toInt(22);
    p.user = o.value(QStringLiteral("user")).toString();
    p.savePassword = o.value(QStringLiteral("savePassword")).toBool(false);
    p.privateKeyPath = o.value(QStringLiteral("privateKeyPath")).toString();
    p.privateKeyId = o.value(QStringLiteral("privateKeyId")).toString();
    p.saveKeyPassphrase = o.value(QStringLiteral("saveKeyPassphrase")).toBool(false);
    p.connectionMode = connectionModeFromString(
        o.value(QStringLiteral("connectionMode")).toString(QStringLiteral("ssh")));
    if (p.id.isEmpty()) {
        p.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    return p;
}

} // namespace VaultPrivate

inline void saveProfiles(const QVector<SessionProfile>& profiles)
{
    // 1) Persist non-sensitive metadata to the encrypted connects.json.
    QJsonArray arr;
    for (const SessionProfile& p : profiles) {
        arr.append(VaultPrivate::profileToJson(p));
    }
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("profiles"), arr);
    const QByteArray plain = QJsonDocument(root).toJson(QJsonDocument::Compact);

    VaultManager vault;
    vault.saveConnectsJson(plain);

    // 2) Store/remove sensitive fields in the keyring-protected dbvault.
    for (const SessionProfile& p : profiles) {
        if (p.savePassword) {
            vault.storeSecret(p.id, QStringLiteral("password"), p.password.toUtf8());
        } else {
            vault.removeSecret(p.id, QStringLiteral("password"));
        }
        if (p.saveKeyPassphrase) {
            vault.storeSecret(p.id, QStringLiteral("keyPassphrase"), p.keyPassphrase.toUtf8());
        } else {
            vault.removeSecret(p.id, QStringLiteral("keyPassphrase"));
        }
    }
}

inline QVector<SessionProfile> loadProfiles()
{
    VaultManager vault;

    QByteArray plain;
    const VaultManager::LoadOutcome oc = vault.loadConnectsJson(plain);

    // First run / nothing saved yet (or legacy QSettings fallback on corruption):
    // migrate any legacy QSettings profiles into the encrypted vault.
    if (oc == VaultManager::LoadOutcome::NotFound || oc == VaultManager::LoadOutcome::Corrupt) {
        QVector<SessionProfile> legacy = profilesFromQSettings();
        if (!legacy.isEmpty()) {
            saveProfiles(legacy);
        }
        return legacy;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(plain);
    if (doc.isNull() || !doc.isObject()) {
        return {};
    }
    const QJsonArray arr = doc.object().value(QStringLiteral("profiles")).toArray();

    QVector<SessionProfile> out;
    out.reserve(arr.size());
    for (const QJsonValue& v : arr) {
        SessionProfile p = VaultPrivate::profileFromJson(v.toObject());
        if (p.id.isEmpty()) {
            continue;
        }
        // Pull sensitive material from the keyring-protected dbvault.
        QByteArray secret;
        if (p.savePassword && vault.retrieveSecret(p.id, QStringLiteral("password"), secret)) {
            p.password = QString::fromUtf8(secret);
            secret.fill('\0');
        }
        if (p.saveKeyPassphrase
            && vault.retrieveSecret(p.id, QStringLiteral("keyPassphrase"), secret)) {
            p.keyPassphrase = QString::fromUtf8(secret);
            secret.fill('\0');
        }
        out.push_back(p);
    }
    return out;
}

inline SessionProfile makeProfile(const QString& host, int port, const QString& user,
                                  const QString& password, const QString& name = {})
{
    SessionProfile p;
    p.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    p.host = host;
    p.port = port;
    p.user = user;
    p.password = password;
    p.name = name;
    return p;
}
