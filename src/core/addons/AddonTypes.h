#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

/** Platform + Qt ABI tag used to pick the right binary from the catalog. */
QString clientoshAddonAbi();

struct AddonArtifact {
    QString abi;
    QString url;
    QString sha256;
    qint64 size = 0;
};

struct AddonCatalogEntry {
    QString id;
    QString name;
    QString description;
    QString version;
    QString author;
    QString homepage;
    QVector<AddonArtifact> artifacts;

    /** Artifact matching the running app ABI, or null-like empty url. */
    AddonArtifact artifactForThisPlatform() const;
    bool hasCompatibleArtifact() const;
};

struct AddonCatalog {
    int format = 1;
    QString updated;
    QVector<AddonCatalogEntry> addons;
};

struct AddonInstallRecord {
    QString id;
    QString name;
    QString version;
    QString author;
    QString description;
    QString pluginFile; // filename inside the addon folder
    QString sha256;
    QString abi;
    bool enabled = true;
    qint64 installedAtMs = 0;
};
