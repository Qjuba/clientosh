#pragma once

#include <QString>
#include <QStringList>

/**
 * Local preferences for the addon marketplace (repository URL, enabled flags).
 * Installed files live under AddonStore::addonsRoot(), not in QSettings.
 */
namespace AddonConfig {

/** Default public catalog URL (raw index.json). */
QString defaultRepositoryUrl();

QString repositoryUrl();
void setRepositoryUrl(const QString& url);

/** Per-addon enable flag (even when files are on disk). */
bool isEnabled(const QString& addonId);
void setEnabled(const QString& addonId, bool on);

QStringList enabledAddonIds();

} // namespace AddonConfig
