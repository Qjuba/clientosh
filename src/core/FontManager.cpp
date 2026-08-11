#include "FontManager.h"

#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QUrl>

namespace {
FontManager* g_instance = nullptr;

QList<FontCatalogEntry> buildCatalog()
{
    // latin-400 static TTFs via jsDelivr / fontsource (small, reliable).
    return {
        {QStringLiteral("inter"), QStringLiteral("Inter"), QStringLiteral("Inter"),
         QStringLiteral("https://cdn.jsdelivr.net/fontsource/fonts/inter@5.2.5/latin-400-normal.ttf"),
         QStringLiteral("Inter-400.ttf"), false},
        {QStringLiteral("ibm-plex-sans"), QStringLiteral("IBM Plex Sans"), QStringLiteral("IBM Plex Sans"),
         QStringLiteral(
             "https://cdn.jsdelivr.net/fontsource/fonts/ibm-plex-sans@5.2.5/latin-400-normal.ttf"),
         QStringLiteral("IBMPlexSans-400.ttf"), false},
        {QStringLiteral("source-sans-3"), QStringLiteral("Source Sans 3"), QStringLiteral("Source Sans 3"),
         QStringLiteral(
             "https://cdn.jsdelivr.net/fontsource/fonts/source-sans-3@5.2.5/latin-400-normal.ttf"),
         QStringLiteral("SourceSans3-400.ttf"), false},
        {QStringLiteral("nunito-sans"), QStringLiteral("Nunito Sans"), QStringLiteral("Nunito Sans"),
         QStringLiteral(
             "https://cdn.jsdelivr.net/fontsource/fonts/nunito-sans@5.2.5/latin-400-normal.ttf"),
         QStringLiteral("NunitoSans-400.ttf"), false},
        {QStringLiteral("jetbrains-mono"), QStringLiteral("JetBrains Mono"),
         QStringLiteral("JetBrains Mono"),
         QStringLiteral(
             "https://cdn.jsdelivr.net/fontsource/fonts/jetbrains-mono@5.2.5/latin-400-normal.ttf"),
         QStringLiteral("JetBrainsMono-400.ttf"), true},
        {QStringLiteral("fira-code"), QStringLiteral("Fira Code"), QStringLiteral("Fira Code"),
         QStringLiteral(
             "https://cdn.jsdelivr.net/fontsource/fonts/fira-code@5.2.5/latin-400-normal.ttf"),
         QStringLiteral("FiraCode-400.ttf"), true},
        {QStringLiteral("source-code-pro"), QStringLiteral("Source Code Pro"),
         QStringLiteral("Source Code Pro"),
         QStringLiteral(
             "https://cdn.jsdelivr.net/fontsource/fonts/source-code-pro@5.2.5/latin-400-normal.ttf"),
         QStringLiteral("SourceCodePro-400.ttf"), true},
        {QStringLiteral("ibm-plex-mono"), QStringLiteral("IBM Plex Mono"), QStringLiteral("IBM Plex Mono"),
         QStringLiteral(
             "https://cdn.jsdelivr.net/fontsource/fonts/ibm-plex-mono@5.2.5/latin-400-normal.ttf"),
         QStringLiteral("IBMPlexMono-400.ttf"), true},
    };
}
} // namespace

FontManager* FontManager::instance()
{
    if (!g_instance) {
        g_instance = new FontManager;
    }
    return g_instance;
}

FontManager::FontManager(QObject* parent)
    : QObject(parent)
    , m_catalog(buildCatalog())
{
}

QString FontManager::fontsDir() const
{
    const QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return root + QStringLiteral("/fonts");
}

void FontManager::loadCachedFonts()
{
    QDir dir(fontsDir());
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
    for (const FontCatalogEntry& entry : m_catalog) {
        const QString path = dir.filePath(entry.fileName);
        if (QFile::exists(path)) {
            registerFontFile(path, entry.family);
        }
    }
}

QList<FontCatalogEntry> FontManager::uiCatalog() const
{
    QList<FontCatalogEntry> out;
    for (const FontCatalogEntry& e : m_catalog) {
        if (!e.monospace) {
            out.push_back(e);
        }
    }
    return out;
}

QList<FontCatalogEntry> FontManager::terminalCatalog() const
{
    QList<FontCatalogEntry> out;
    for (const FontCatalogEntry& e : m_catalog) {
        if (e.monospace) {
            out.push_back(e);
        }
    }
    return out;
}

bool FontManager::isFamilyLoaded(const QString& family) const
{
    if (family.trimmed().isEmpty()) {
        return true;
    }
    if (m_loadedIds.contains(family)) {
        return true;
    }
    // Already installed on the system.
    return QFontDatabase::hasFamily(family);
}

bool FontManager::isCatalogFont(const QString& family) const
{
    return findByFamily(family) != nullptr;
}

bool FontManager::needsDownload(const QString& family) const
{
    if (family.trimmed().isEmpty()) {
        return false;
    }
    if (isFamilyLoaded(family)) {
        return false;
    }
    return isCatalogFont(family);
}

bool FontManager::isDownloading(const QString& family) const
{
    return m_inflight.contains(family);
}

const FontCatalogEntry* FontManager::findByFamily(const QString& family) const
{
    for (const FontCatalogEntry& e : m_catalog) {
        if (e.family.compare(family, Qt::CaseInsensitive) == 0) {
            return &e;
        }
    }
    return nullptr;
}

bool FontManager::registerFontFile(const QString& path, const QString& expectedFamily)
{
    const int id = QFontDatabase::addApplicationFont(path);
    if (id < 0) {
        return false;
    }
    const QStringList families = QFontDatabase::applicationFontFamilies(id);
    QString resolved = expectedFamily;
    if (!families.isEmpty()) {
        resolved = families.first();
    }
    m_loadedIds.insert(resolved, id);
    if (resolved.compare(expectedFamily, Qt::CaseInsensitive) != 0) {
        m_loadedIds.insert(expectedFamily, id);
    }
    return true;
}

void FontManager::ensureFamily(const QString& family)
{
    if (family.trimmed().isEmpty() || isFamilyLoaded(family)) {
        emit fontReady(family);
        return;
    }
    const FontCatalogEntry* entry = findByFamily(family);
    if (!entry) {
        emit fontFailed(family, QStringLiteral("font is not available"));
        return;
    }
    if (m_inflight.contains(family)) {
        return;
    }

    const QString path = QDir(fontsDir()).filePath(entry->fileName);
    if (QFile::exists(path)) {
        if (registerFontFile(path, entry->family)) {
            emit fontReady(entry->family);
            return;
        }
        QFile::remove(path);
    }
    startDownload(*entry);
}

void FontManager::startDownload(const FontCatalogEntry& entry)
{
    QDir().mkpath(fontsDir());
    emit downloadStarted(entry.family);

    QNetworkRequest req{QUrl(entry.url)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setRawHeader("User-Agent", "clientosh/1.0");

    QNetworkReply* reply = m_nam.get(req);
    m_inflight.insert(entry.family, reply);

    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, family = entry.family](qint64 received, qint64 total) {
                emit downloadProgress(family, received, total);
            });

    connect(reply, &QNetworkReply::finished, this, [this, entry, reply]() {
        m_inflight.remove(entry.family);
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit fontFailed(entry.family, reply->errorString());
            return;
        }

        const QByteArray data = reply->readAll();
        if (data.size() < 1024) {
            emit fontFailed(entry.family, QStringLiteral("download too small / invalid"));
            return;
        }

        const QString path = QDir(fontsDir()).filePath(entry.fileName);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            emit fontFailed(entry.family, QStringLiteral("cannot write font cache"));
            return;
        }
        if (file.write(data) != data.size()) {
            file.close();
            QFile::remove(path);
            emit fontFailed(entry.family, QStringLiteral("failed writing font file"));
            return;
        }
        file.close();

        if (!registerFontFile(path, entry.family)) {
            QFile::remove(path);
            emit fontFailed(entry.family, QStringLiteral("failed to register font"));
            return;
        }
        emit fontReady(entry.family);
    });
}
