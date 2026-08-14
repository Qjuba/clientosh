#include "SyncConfig.h"

#include "SyncKey.h"
#include "KeyringAdapter.h"

#include <QSettings>
#include <QString>
#include <QtGlobal>

namespace {

const char* kEnabled = "sync/enabled";
const char* kSyncKey = "sync/syncKey";
const char* kGistDesc = "sync/gistDescription";
const char* kPollInterval = "sync/pollIntervalSec";
const char* kLastRev = "sync/lastKnownRev";

inline QString tokenKeyName(const QString& uuidHex)
{
    return KeyringAdapter::servicePrefix() + QStringLiteral(".sync.token.") + uuidHex;
}

} // namespace

namespace SyncConfig {

bool enabled()
{
    return QSettings().value(QLatin1String(kEnabled), false).toBool();
}

void setEnabled(bool on)
{
    QSettings s;
    s.setValue(QLatin1String(kEnabled), on);
    s.sync();
}

QString syncKeyText()
{
    return QSettings().value(QLatin1String(kSyncKey), QString()).toString();
}

void setSyncKeyText(const QString& text)
{
    QSettings s;
    s.setValue(QLatin1String(kSyncKey), text);
    s.sync();
}

SyncKey syncKey()
{
    return SyncKeyCodec::decode(syncKeyText());
}

QString gistDescription()
{
    return QSettings().value(QLatin1String(kGistDesc), QString()).toString();
}

void setGistDescription(const QString& text)
{
    QSettings s;
    s.setValue(QLatin1String(kGistDesc), text);
    s.sync();
}

int pollIntervalSec()
{
    return qBound(10, QSettings().value(QLatin1String(kPollInterval), 60).toInt(), 3600);
}

void setPollIntervalSec(int seconds)
{
    QSettings s;
    s.setValue(QLatin1String(kPollInterval), qBound(10, seconds, 3600));
    s.sync();
}

int lastKnownRev()
{
    return QSettings().value(QLatin1String(kLastRev), 0).toInt();
}

void setLastKnownRev(int rev)
{
    QSettings s;
    s.setValue(QLatin1String(kLastRev), rev);
    s.sync();
}

void storeToken(const QString& uuidHex, const QString& token)
{
    KeyringAdapter::store(tokenKeyName(uuidHex), token.toUtf8());
}

QString loadToken(const QString& uuidHex)
{
    QByteArray data;
    if (KeyringAdapter::retrieve(tokenKeyName(uuidHex), data)) {
        const QString token = QString::fromUtf8(data);
        data.fill('\0');
        return token;
    }
    return QString();
}

void clearToken(const QString& uuidHex)
{
    KeyringAdapter::remove(tokenKeyName(uuidHex));
}

} // namespace SyncConfig
