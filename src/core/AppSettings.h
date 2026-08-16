#pragma once

#include <QColor>
#include <QKeySequence>
#include <QSettings>
#include <QString>
#include <QVariant>

/** Central QSettings keys for clientosh preferences. */
namespace AppSettings {

inline constexpr const char* kSavePasswordDefault = "settings/savePasswordDefault";
inline constexpr const char* kHideDotfiles = "settings/hideDotfiles";
inline constexpr const char* kAnimationsEnabled = "settings/animationsEnabled";
inline constexpr const char* kFontSize = "settings/fontSize";
inline constexpr const char* kFontFamily = "settings/fontFamily";
inline constexpr const char* kUiFontSize = "settings/uiFontSize";
inline constexpr const char* kUiFontFamily = "settings/uiFontFamily";
inline constexpr const char* kTheme = "settings/theme";
inline constexpr const char* kTerminalFg = "settings/terminalFg";
inline constexpr const char* kTerminalBg = "settings/terminalBg";
inline constexpr const char* kTerminalBgImage = "settings/terminalBgImage";
inline constexpr const char* kTerminalBgOpacity = "settings/terminalBgOpacity";
inline constexpr const char* kTerminalBgBlur = "settings/terminalBgBlur";
inline constexpr const char* kDefaultHost = "settings/defaultHost";
inline constexpr const char* kDefaultUser = "settings/defaultUser";
inline constexpr const char* kDefaultPort = "settings/defaultPort";
inline constexpr const char* kStatsIntervalSec = "settings/statsIntervalSec";
inline constexpr const char* kShowServerStats = "settings/showServerStats";
inline constexpr const char* kSftpDefaultView = "settings/sftpDefaultView";
inline constexpr const char* kSftpVerboseLogging = "settings/sftpVerboseLogging";
inline constexpr const char* kHighlightAddresses = "settings/highlightAddresses";
inline constexpr const char* kHighlightLogKeywords = "settings/highlightLogKeywords";
inline constexpr const char* kCtrlScrollFontZoom = "settings/ctrlScrollFontZoom";
inline constexpr const char* kScrollSensitivity = "settings/scrollSensitivity";
inline constexpr const char* kCopyPasteMode = "settings/copyPasteMode";
inline constexpr const char* kShortcutNewSession = "settings/shortcutNewSession";
inline constexpr const char* kShortcutSettings = "settings/shortcutSettings";
inline constexpr const char* kShortcutDashboard = "settings/shortcutDashboard";
inline constexpr const char* kShortcutClosePanel = "settings/shortcutClosePanel";
inline constexpr const char* kShortcutOpenSftp = "settings/shortcutOpenSftp";
inline constexpr const char* kShortcutFontLarger = "settings/shortcutFontLarger";
inline constexpr const char* kShortcutFontSmaller = "settings/shortcutFontSmaller";
inline constexpr const char* kShortcutFontReset = "settings/shortcutFontReset";
inline constexpr const char* kShortcutNewSessionEnabled = "settings/shortcutNewSessionEnabled";
inline constexpr const char* kShortcutSettingsEnabled = "settings/shortcutSettingsEnabled";
inline constexpr const char* kShortcutDashboardEnabled = "settings/shortcutDashboardEnabled";
inline constexpr const char* kShortcutClosePanelEnabled = "settings/shortcutClosePanelEnabled";
inline constexpr const char* kShortcutOpenSftpEnabled = "settings/shortcutOpenSftpEnabled";
inline constexpr const char* kShortcutFontLargerEnabled = "settings/shortcutFontLargerEnabled";
inline constexpr const char* kShortcutFontSmallerEnabled = "settings/shortcutFontSmallerEnabled";
inline constexpr const char* kShortcutFontResetEnabled = "settings/shortcutFontResetEnabled";

inline QColor colorFromSetting(const char* key, const QColor& fallback)
{
    const QString hex = QSettings().value(QLatin1String(key)).toString().trimmed();
    if (hex.isEmpty()) {
        return fallback;
    }
    const QColor c(hex);
    return c.isValid() ? c : fallback;
}

inline void setValueSync(const char* key, const QVariant& value)
{
    QSettings s;
    s.setValue(QLatin1String(key), value);
    s.sync();
}

inline void setColorSetting(const char* key, const QColor& color)
{
    setValueSync(key, color.name(QColor::HexRgb));
}

inline bool savePasswordDefault()
{
    return QSettings().value(QLatin1String(kSavePasswordDefault), false).toBool();
}

inline bool hideDotfiles()
{
    return QSettings().value(QLatin1String(kHideDotfiles), true).toBool();
}

inline bool animationsEnabled()
{
    return QSettings().value(QLatin1String(kAnimationsEnabled), true).toBool();
}

inline int fontSize()
{
    return qBound(9, QSettings().value(QLatin1String(kFontSize), 11).toInt(), 22);
}

inline void setFontSize(int points)
{
    const int next = qBound(9, points, 22);
    QSettings s;
    s.setValue(QLatin1String(kFontSize), next);
    s.sync();
}

/** Empty = auto-pick a monospace face. */
inline QString fontFamily()
{
    return QSettings().value(QLatin1String(kFontFamily), QString()).toString().trimmed();
}

inline int uiFontSize()
{
    return qBound(9, QSettings().value(QLatin1String(kUiFontSize), 10).toInt(), 22);
}

/** Empty = system UI default. */
inline QString uiFontFamily()
{
    return QSettings().value(QLatin1String(kUiFontFamily), QString()).toString().trimmed();
}

inline QString theme()
{
    return QSettings().value(QLatin1String(kTheme), QStringLiteral("dark")).toString();
}

inline bool isLightTheme()
{
    return theme().compare(QLatin1String("light"), Qt::CaseInsensitive) == 0;
}

inline QColor defaultTerminalFgForTheme(bool light)
{
    return light ? QColor(0x1a, 0x1a, 0x1a) : QColor(0xc8, 0xc8, 0xc8);
}

inline QColor defaultTerminalBgForTheme(bool light)
{
    return light ? QColor(0xff, 0xff, 0xff) : QColor(0x1a, 0x1a, 0x1a);
}

inline QColor terminalFg()
{
    return colorFromSetting(kTerminalFg, defaultTerminalFgForTheme(isLightTheme()));
}

inline QColor terminalBg()
{
    return colorFromSetting(kTerminalBg, defaultTerminalBgForTheme(isLightTheme()));
}

inline QString terminalBgImage()
{
    return QSettings().value(QLatin1String(kTerminalBgImage), QString()).toString();
}

inline void setTerminalBgImage(const QString& imagePath)
{
    QSettings().setValue(QLatin1String(kTerminalBgImage), imagePath);
}

inline qreal terminalBgOpacity()
{
    return qBound(0.0, QSettings().value(QLatin1String(kTerminalBgOpacity), 0.5).toReal(), 1.0);
}

inline void setTerminalBgOpacity(qreal opacity)
{
    QSettings().setValue(QLatin1String(kTerminalBgOpacity), qBound(0.0, opacity, 1.0));
}

inline int terminalBgBlur()
{
    return qBound(0, QSettings().value(QLatin1String(kTerminalBgBlur), 0).toInt(), 100);
}

inline void setTerminalBgBlur(int radius)
{
    QSettings().setValue(QLatin1String(kTerminalBgBlur), qBound(0, radius, 100));
}

inline void resetTerminalAppearance()
{
    QSettings s;
    s.remove(QLatin1String(kTerminalFg));
    s.remove(QLatin1String(kTerminalBg));
    s.remove(QLatin1String(kTerminalBgImage));
    s.remove(QLatin1String(kTerminalBgOpacity));
    s.remove(QLatin1String(kTerminalBgBlur));
}

inline QString defaultHost()
{
    return QSettings().value(QLatin1String(kDefaultHost), QStringLiteral("127.0.0.1")).toString();
}

inline QString defaultUser()
{
    return QSettings().value(QLatin1String(kDefaultUser), QString()).toString();
}

inline int defaultPort()
{
    return qBound(1, QSettings().value(QLatin1String(kDefaultPort), 22).toInt(), 65535);
}

inline int statsIntervalSec()
{
    return qBound(1, QSettings().value(QLatin1String(kStatsIntervalSec), 2).toInt(), 30);
}

inline bool showServerStats()
{
    return QSettings().value(QLatin1String(kShowServerStats), true).toBool();
}

/** "details" (name/size/type) or "compact" (name/size). */
inline QString sftpDefaultView()
{
    const QString v = QSettings().value(QLatin1String(kSftpDefaultView), QStringLiteral("details")).toString();
    return (v == QLatin1String("compact")) ? QStringLiteral("compact") : QStringLiteral("details");
}

inline bool sftpCompactView()
{
    return sftpDefaultView() == QLatin1String("compact");
}

inline bool sftpVerboseLogging()
{
    return QSettings().value(QLatin1String(kSftpVerboseLogging), false).toBool();
}

inline void setSftpVerboseLogging(bool enabled)
{
    setValueSync(kSftpVerboseLogging, enabled);
}

inline bool highlightAddresses()
{
    return QSettings().value(QLatin1String(kHighlightAddresses), true).toBool();
}

inline bool highlightLogKeywords()
{
    return QSettings().value(QLatin1String(kHighlightLogKeywords), true).toBool();
}

inline bool ctrlScrollFontZoom()
{
    return QSettings().value(QLatin1String(kCtrlScrollFontZoom), true).toBool();
}

/** Lines scrolled per wheel notch (1–20). Default 1 line per notch. */
inline int scrollSensitivity()
{
    return qBound(1, QSettings().value(QLatin1String(kScrollSensitivity), 1).toInt(), 20);
}

inline void setScrollSensitivity(int lines)
{
    setValueSync(kScrollSensitivity, qBound(1, lines, 20));
}

/** "standard" or "menu". */
inline QString copyPasteMode()
{
    const QString v = QSettings().value(QLatin1String(kCopyPasteMode), QStringLiteral("standard")).toString();
    return (v == QLatin1String("menu")) ? QStringLiteral("menu") : QStringLiteral("standard");
}

inline bool copyPasteMenu()
{
    return copyPasteMode() == QLatin1String("menu");
}

inline void setCopyPasteMode(const QString& mode)
{
    const QString next = (mode == QLatin1String("menu")) ? QStringLiteral("menu")
                                                         : QStringLiteral("standard");
    setValueSync(kCopyPasteMode, next);
}

inline QKeySequence shortcutFromSetting(const char* key, const QKeySequence& fallback)
{
    const QString stored = QSettings().value(QLatin1String(key)).toString().trimmed();
    if (stored.isEmpty()) {
        return fallback;
    }
    const QKeySequence seq = QKeySequence::fromString(stored, QKeySequence::PortableText);
    return seq.isEmpty() ? fallback : seq;
}

inline void setShortcutSetting(const char* key, const QKeySequence& seq)
{
    setValueSync(key, seq.toString(QKeySequence::PortableText));
}

inline void setShortcutEnabled(const char* key, bool enabled)
{
    setValueSync(key, enabled);
}

inline bool shortcutEnabled(const char* key)
{
    // A shortcut is enabled by default unless explicitly disabled by the user.
    return QSettings().value(QLatin1String(key), true).toBool();
}

inline QKeySequence shortcutNewSession()
{
    if (!shortcutEnabled(kShortcutNewSessionEnabled)) {
        return {};
    }
    return shortcutFromSetting(kShortcutNewSession, QKeySequence(QStringLiteral("Ctrl+N")));
}

inline QKeySequence shortcutSettings()
{
    if (!shortcutEnabled(kShortcutSettingsEnabled)) {
        return {};
    }
    return shortcutFromSetting(kShortcutSettings, QKeySequence(QStringLiteral("Ctrl+,")));
}

inline QKeySequence shortcutDashboard()
{
    if (!shortcutEnabled(kShortcutDashboardEnabled)) {
        return {};
    }
    return shortcutFromSetting(kShortcutDashboard, QKeySequence(QStringLiteral("Ctrl+Shift+D")));
}

inline QKeySequence shortcutClosePanel()
{
    if (!shortcutEnabled(kShortcutClosePanelEnabled)) {
        return {};
    }
    return shortcutFromSetting(kShortcutClosePanel, QKeySequence(QStringLiteral("Ctrl+W")));
}

inline QKeySequence shortcutOpenSftp()
{
    if (!shortcutEnabled(kShortcutOpenSftpEnabled)) {
        return {};
    }
    return shortcutFromSetting(kShortcutOpenSftp, QKeySequence(QStringLiteral("Ctrl+Shift+S")));
}

inline QKeySequence shortcutFontLarger()
{
    if (!shortcutEnabled(kShortcutFontLargerEnabled)) {
        return {};
    }
    return shortcutFromSetting(kShortcutFontLarger, QKeySequence(QStringLiteral("Ctrl+=")));
}

inline QKeySequence shortcutFontSmaller()
{
    if (!shortcutEnabled(kShortcutFontSmallerEnabled)) {
        return {};
    }
    return shortcutFromSetting(kShortcutFontSmaller, QKeySequence(QStringLiteral("Ctrl+-")));
}

inline QKeySequence shortcutFontReset()
{
    if (!shortcutEnabled(kShortcutFontResetEnabled)) {
        return {};
    }
    return shortcutFromSetting(kShortcutFontReset, QKeySequence(QStringLiteral("Ctrl+0")));
}

} // namespace AppSettings
