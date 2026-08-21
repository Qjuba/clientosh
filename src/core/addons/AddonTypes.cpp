#include "AddonTypes.h"

#include <QtGlobal>

QString clientoshAddonAbi()
{
    // Keep in sync with release CI toolchain labels (Qt minor is enough for matching).
#if defined(Q_OS_WIN)
    return QStringLiteral("qt%1.%2-win64-mingw")
        .arg(QT_VERSION_MAJOR)
        .arg(QT_VERSION_MINOR);
#elif defined(Q_OS_MACOS)
    return QStringLiteral("qt%1.%2-macos")
        .arg(QT_VERSION_MAJOR)
        .arg(QT_VERSION_MINOR);
#else
    return QStringLiteral("qt%1.%2-linux-x64")
        .arg(QT_VERSION_MAJOR)
        .arg(QT_VERSION_MINOR);
#endif
}

AddonArtifact AddonCatalogEntry::artifactForThisPlatform() const
{
    const QString want = clientoshAddonAbi();
    for (const AddonArtifact& a : artifacts) {
        if (a.abi.compare(want, Qt::CaseInsensitive) == 0 && !a.url.isEmpty()) {
            return a;
        }
    }
    return {};
}

bool AddonCatalogEntry::hasCompatibleArtifact() const
{
    return !artifactForThisPlatform().url.isEmpty();
}
