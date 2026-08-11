#pragma once

#include <QString>
#include <QVector>
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
    QString privateKeyPath; // if set, prefer public-key auth
    QString keyPassphrase;  // for encrypted keys (not persisted by default)
    bool saveKeyPassphrase = false;
    ConnectionMode connectionMode = ConnectionMode::Ssh;

    bool usesPrivateKey() const
    {
        return !privateKeyPath.trimmed().isEmpty();
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

inline QVector<SessionProfile> loadProfiles()
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

inline void saveProfiles(const QVector<SessionProfile>& profiles)
{
    QSettings s;
    s.beginWriteArray(QStringLiteral("profiles"));
    for (int i = 0; i < profiles.size(); ++i) {
        s.setArrayIndex(i);
        const SessionProfile& p = profiles[i];
        s.setValue(QStringLiteral("id"), p.id);
        s.setValue(QStringLiteral("name"), p.name);
        s.setValue(QStringLiteral("host"), p.host);
        s.setValue(QStringLiteral("port"), p.port);
        s.setValue(QStringLiteral("user"), p.user);
        s.setValue(QStringLiteral("savePassword"), p.savePassword);
        if (p.savePassword) {
            s.setValue(QStringLiteral("password"), p.password);
        } else {
            s.remove(QStringLiteral("password"));
        }
        s.setValue(QStringLiteral("privateKeyPath"), p.privateKeyPath);
        s.setValue(QStringLiteral("saveKeyPassphrase"), p.saveKeyPassphrase);
        if (p.saveKeyPassphrase) {
            s.setValue(QStringLiteral("keyPassphrase"), p.keyPassphrase);
        } else {
            s.remove(QStringLiteral("keyPassphrase"));
        }
        s.setValue(QStringLiteral("connectionMode"), connectionModeToString(p.connectionMode));
    }
    s.endArray();
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
