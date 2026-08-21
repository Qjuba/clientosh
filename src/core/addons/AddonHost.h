#pragma once

#include "AddonTypes.h"

#include <QObject>
#include <QString>
#include <QVector>

class AddonStore;

/**
 * Process-side addon coordinator.
 *
 * Today: tracks installed/enabled addons from AddonStore (no QPluginLoader yet).
 * Later: will load plugin binaries for enabled installs only.
 */
class AddonHost : public QObject
{
    Q_OBJECT

public:
    explicit AddonHost(AddonStore* store, QObject* parent = nullptr);

    AddonStore* store() const { return m_store; }

    /** Installed addons that are marked enabled (will be loadable later). */
    QVector<AddonInstallRecord> enabledInstalled() const;

    /** Refresh local install list; does not load plugins yet. */
    void reloadInstalled();

    void setAddonEnabled(const QString& addonId, bool enabled);

signals:
    void installedChanged();
    void statusMessage(const QString& message);

private:
    AddonStore* m_store = nullptr;
    QVector<AddonInstallRecord> m_installed;
};
