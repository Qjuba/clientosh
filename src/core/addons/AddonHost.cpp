#include "AddonHost.h"

#include "AddonConfig.h"
#include "AddonStore.h"

AddonHost::AddonHost(AddonStore* store, QObject* parent)
    : QObject(parent)
    , m_store(store)
{
    reloadInstalled();
    if (m_store) {
        connect(m_store, &AddonStore::installFinished, this,
                [this](const QString&, bool ok, const QString&) {
                    if (ok) {
                        reloadInstalled();
                    }
                });
        connect(m_store, &AddonStore::removeFinished, this,
                [this](const QString&, bool ok, const QString&) {
                    if (ok) {
                        reloadInstalled();
                    }
                });
    }
}

QVector<AddonInstallRecord> AddonHost::enabledInstalled() const
{
    QVector<AddonInstallRecord> out;
    for (const AddonInstallRecord& r : m_installed) {
        if (AddonConfig::isEnabled(r.id)) {
            out.append(r);
        }
    }
    return out;
}

void AddonHost::reloadInstalled()
{
    m_installed = m_store ? m_store->installed() : QVector<AddonInstallRecord>{};
    emit installedChanged();
    const int enabled = enabledInstalled().size();
    emit statusMessage(QStringLiteral("%1 addon(s) installed, %2 enabled (loading not active yet).")
                           .arg(m_installed.size())
                           .arg(enabled));
}

void AddonHost::setAddonEnabled(const QString& addonId, bool enabled)
{
    AddonConfig::setEnabled(addonId, enabled);
    reloadInstalled();
    // Plugin load/unload will hook in here later.
    emit statusMessage(enabled ? QStringLiteral("Addon \"%1\" enabled (will load when plugin support ships).")
                                     .arg(addonId)
                               : QStringLiteral("Addon \"%1\" disabled.").arg(addonId));
}
