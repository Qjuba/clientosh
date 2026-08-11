#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>
#include <QNetworkAccessManager>

class QNetworkReply;

struct FontCatalogEntry {
    QString id;          // stable key, e.g. "inter"
    QString family;      // QFont family name after install
    QString label;       // UI label
    QString url;         // download URL (ttf/otf)
    QString fileName;    // cached file name
    bool monospace = false;
};

/**
 * Registers bundled-on-demand webfonts (Inter, JetBrains Mono, …)
 * into QFontDatabase. Downloads into AppData/fonts when selected.
 */
class FontManager : public QObject
{
    Q_OBJECT

public:
    static FontManager* instance();

    void loadCachedFonts();

    QList<FontCatalogEntry> uiCatalog() const;
    QList<FontCatalogEntry> terminalCatalog() const;

    bool isFamilyLoaded(const QString& family) const;
    bool isCatalogFont(const QString& family) const;
    bool needsDownload(const QString& family) const;
    bool isDownloading(const QString& family) const;

    /** Ensure font file is present and registered. Emits fontReady / fontFailed. */
    void ensureFamily(const QString& family);

signals:
    void fontReady(const QString& family);
    void fontFailed(const QString& family, const QString& error);
    void downloadStarted(const QString& family);
    void downloadProgress(const QString& family, qint64 received, qint64 total);

private:
    explicit FontManager(QObject* parent = nullptr);

    QString fontsDir() const;
    const FontCatalogEntry* findByFamily(const QString& family) const;
    bool registerFontFile(const QString& path, const QString& expectedFamily);
    void startDownload(const FontCatalogEntry& entry);

    QNetworkAccessManager m_nam;
    QList<FontCatalogEntry> m_catalog;
    QHash<QString, int> m_loadedIds; // family → fontId
    QHash<QString, QNetworkReply*> m_inflight; // family → reply
};
