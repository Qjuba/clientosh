#pragma once

#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QFontMetrics>
#include <QStringList>

inline bool clientoshFontIsMonospace(const QFont& font)
{
    const QFontMetrics fm(font);
    const int wM = fm.horizontalAdvance(QLatin1Char('M'));
    if (wM <= 0) {
        return false;
    }
    // Require identical advances across several glyphs — do not trust fixedPitch flags.
    return wM == fm.horizontalAdvance(QLatin1Char('i'))
        && wM == fm.horizontalAdvance(QLatin1Char('W'))
        && wM == fm.horizontalAdvance(QLatin1Char('.'))
        && wM == fm.horizontalAdvance(QLatin1Char('='))
        && wM == fm.horizontalAdvance(QLatin1Char(' '));
}

inline QFont clientoshPrepareMonoFont(const QString& family, int pointSize)
{
    QFont f(family);
    f.setStyleHint(QFont::TypeWriter);
    // Allow font merging so missing glyphs (Braille, symbols) can fall back on Linux.
    // Cells are drawn at fixed positions, so fallback advance width is not critical.
    f.setStyleStrategy(QFont::PreferDefault);
    f.setFixedPitch(true);
    f.setKerning(false);
    f.setHintingPreference(QFont::PreferFullHinting);
    f.setPointSize(pointSize);
    return f;
}

/** Installed monospace families suitable for the terminal (sorted). */
inline QStringList clientoshMonospaceFamilies()
{
    QStringList out;
    const QStringList families = QFontDatabase::families();
    for (const QString& family : families) {
        if (!QFontDatabase::isFixedPitch(family)) {
            continue;
        }
        QFont candidate = clientoshPrepareMonoFont(family, 12);
        if (clientoshFontIsMonospace(candidate)) {
            out.push_back(family);
        }
    }
    out.removeDuplicates();
    out.sort(Qt::CaseInsensitive);
    return out;
}

/**
 * Build a monospace font. If preferredFamily is set and valid, use it;
 * otherwise fall back through platform preferences then any fixed-pitch face.
 */
inline QFont clientoshMonospaceFont(int pointSize = 10, const QString& preferredFamily = {})
{
    if (!preferredFamily.trimmed().isEmpty()) {
        QFont candidate = clientoshPrepareMonoFont(preferredFamily.trimmed(), pointSize);
        // Prefer requested family even if metrics aren't perfectly mono (e.g. Fira Code).
        const QFontInfo info(candidate);
        if (info.family().contains(preferredFamily.trimmed(), Qt::CaseInsensitive)
            || clientoshFontIsMonospace(candidate)) {
            return candidate;
        }
        return candidate;
    }

    const QStringList preferred = {
#ifdef Q_OS_WIN
        QStringLiteral("Cascadia Mono"),
        QStringLiteral("Consolas"),
        QStringLiteral("Lucida Console"),
        QStringLiteral("Courier New"),
#elif defined(Q_OS_MACOS)
        QStringLiteral("Menlo"),
        QStringLiteral("SF Mono"),
        QStringLiteral("Monaco"),
        QStringLiteral("Courier"),
#else
        // Prefer faces that ship Braille (U+2800+) for btop graphs on Linux.
        QStringLiteral("DejaVu Sans Mono"),
        QStringLiteral("Noto Sans Mono"),
        QStringLiteral("FreeMono"),
        QStringLiteral("Liberation Mono"),
        QStringLiteral("Ubuntu Mono"),
        QStringLiteral("Source Code Pro"),
#endif
    };

    for (const QString& family : preferred) {
        QFont candidate = clientoshPrepareMonoFont(family, pointSize);
        const QFontInfo info(candidate);
        if (!info.family().contains(family, Qt::CaseInsensitive)
            && info.family().compare(family, Qt::CaseInsensitive) != 0) {
            if (!clientoshFontIsMonospace(candidate)) {
                continue;
            }
        }
        if (clientoshFontIsMonospace(candidate)) {
            return candidate;
        }
    }

    const QStringList families = clientoshMonospaceFamilies();
    for (const QString& family : families) {
        return clientoshPrepareMonoFont(family, pointSize);
    }

    return clientoshPrepareMonoFont(QFontDatabase::systemFont(QFontDatabase::FixedFont).family(),
                                    pointSize);
}

/** UI / chrome font — proportional faces allowed (Inter, etc.). */
inline QFont clientoshUiFont(int pointSize = 10, const QString& preferredFamily = {})
{
    if (!preferredFamily.trimmed().isEmpty()) {
        QFont f(preferredFamily.trimmed());
        f.setPointSize(pointSize);
        f.setStyleStrategy(QFont::PreferDefault);
        f.setHintingPreference(QFont::PreferFullHinting);
        return f;
    }
    QFont f = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    f.setPointSize(pointSize);
    return f;
}
