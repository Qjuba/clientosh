#include "ui/Motion.h"
#include "DropOverlay.h"
#include "PaneFrame.h"
#include "SessionChip.h"

#include <QAbstractAnimation>
#include <QApplication>
#include <QEnterEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPixmapCache>
#include <QPointer>
#include <QSettings>
#include <QSplitter>
#include <QVector>

#include <algorithm>

namespace Motion {
namespace {
constexpr char kSplitterAnimProp[] = "motionSizesAnim";
constexpr char kSettingsKey[] = "settings/animationsEnabled";

bool g_enabled = true;
bool g_loaded = false;
QVector<QPointer<QVariantAnimation>> g_anims;

void trackAnim(QVariantAnimation* anim)
{
    if (!anim) {
        return;
    }
    g_anims.erase(std::remove_if(g_anims.begin(), g_anims.end(),
                                  [](const QPointer<QVariantAnimation>& p) { return p.isNull(); }),
                  g_anims.end());
    for (const auto& p : g_anims) {
        if (p.data() == anim) {
            return;
        }
    }
    g_anims.push_back(anim);
}

void destroyAnim(QVariantAnimation*& slot)
{
    if (!slot) {
        return;
    }
    slot->stop();
    slot->deleteLater();
    slot = nullptr;
}

void ensureLoaded()
{
    if (g_loaded) {
        return;
    }
    g_loaded = true;
    QSettings s;
    g_enabled = s.value(QLatin1String(kSettingsKey), true).toBool();
}
} // namespace

bool enabled()
{
    ensureLoaded();
    return g_enabled;
}

void loadFromSettings()
{
    g_loaded = true;
    QSettings s;
    g_enabled = s.value(QLatin1String(kSettingsKey), true).toBool();
    if (!g_enabled) {
        releaseCaches();
    }
}

void releaseCaches()
{
    if (qApp) {
        const auto widgets = qApp->allWidgets();
        for (QWidget* w : widgets) {
            if (auto* btn = qobject_cast<HoverFillButton*>(w)) {
                btn->releaseMotionResources();
            } else if (auto* chip = qobject_cast<SessionChip*>(w)) {
                chip->releaseMotionResources();
            } else if (auto* overlay = qobject_cast<DropOverlay*>(w)) {
                overlay->releaseMotionResources();
            } else if (auto* pane = qobject_cast<PaneFrame*>(w)) {
                pane->releaseMotionResources();
            }
        }
    }

    for (const QPointer<QVariantAnimation>& p : g_anims) {
        if (QVariantAnimation* anim = p.data()) {
            anim->stop();
            anim->deleteLater();
        }
    }
    g_anims.clear();

    // No QML engine in this app — reclaim Qt image/pixmap caches instead of
    // trimComponentCache() / QQmlEngine::collectGarbage().
    QPixmapCache::clear();
    QPixmapCache::setCacheLimit(10240); // 10 MB after trim
}

void setEnabled(bool on)
{
    ensureLoaded();
    if (g_enabled == on) {
        if (!on) {
            releaseCaches();
        }
        return;
    }
    g_enabled = on;
    QSettings s;
    s.setValue(QLatin1String(kSettingsKey), on);
    if (!on) {
        releaseCaches();
    }
}

HoverFillButton::HoverFillButton(QWidget* parent)
    : QToolButton(parent)
{
    setAttribute(Qt::WA_Hover, true);
    setAutoRaise(true);
}

void HoverFillButton::setHover(qreal value)
{
    const qreal v = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(m_hover, v)) {
        return;
    }
    m_hover = v;
    update();
}

void HoverFillButton::setHoverFill(const QColor& color)
{
    m_fill = color;
    update();
}

void HoverFillButton::releaseMotionResources()
{
    destroyAnim(m_anim);
    // Snap hover off so no translucent fill stays allocated in paint path intent.
    if (m_hover > 0.0) {
        m_hover = 0.0;
        update();
    }
}

void HoverFillButton::animateHover(qreal target)
{
    if (!enabled()) {
        destroyAnim(m_anim);
        setHover(target);
        return;
    }
    if (!m_anim) {
        m_anim = new QVariantAnimation(this);
        m_anim->setEasingCurve(easeOut());
        connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
            setHover(v.toReal());
        });
        trackAnim(m_anim);
    }
    m_anim->stop();
    m_anim->setDuration(kFastMs);
    m_anim->setStartValue(m_hover);
    m_anim->setEndValue(target);
    m_anim->start();
}

void HoverFillButton::enterEvent(QEnterEvent* event)
{
    animateHover(1.0);
    QToolButton::enterEvent(event);
}

void HoverFillButton::leaveEvent(QEvent* event)
{
    animateHover(0.0);
    QToolButton::leaveEvent(event);
}

void HoverFillButton::paintEvent(QPaintEvent* event)
{
    if (m_hover > 0.01) {
        QPainter p(this);
        QColor c = m_fill;
        c.setAlpha(int(m_hover * 110));
        p.fillRect(rect(), c);
    }
    QToolButton::paintEvent(event);
}

void animateToward(QObject* owner, qreal* value, qreal target, int durationMs,
                   const std::function<void()>& onTick, QVariantAnimation** animSlot)
{
    if (!owner || !value) {
        return;
    }
    target = qBound(0.0, target, 1.0);

    if (!enabled()) {
        if (animSlot) {
            destroyAnim(*animSlot);
        }
        *value = target;
        if (onTick) {
            onTick();
        }
        return;
    }

    QVariantAnimation* anim = animSlot && *animSlot ? *animSlot : nullptr;
    if (!anim) {
        anim = new QVariantAnimation(owner);
        anim->setEasingCurve(easeOut());
        trackAnim(anim);
        if (animSlot) {
            *animSlot = anim;
        }
    }

    if (anim->state() == QAbstractAnimation::Running) {
        anim->stop();
    }

    if (qAbs(*value - target) < 0.001 && anim->state() != QAbstractAnimation::Running) {
        *value = target;
        if (onTick) {
            onTick();
        }
        return;
    }

    QObject::disconnect(anim, &QVariantAnimation::valueChanged, nullptr, nullptr);
    QObject::connect(anim, &QVariantAnimation::valueChanged, owner, [value, onTick](const QVariant& v) {
        *value = v.toReal();
        if (onTick) {
            onTick();
        }
    });
    anim->setDuration(durationMs);
    anim->setStartValue(*value);
    anim->setEndValue(target);
    anim->start();
}

void animateSplitterSizes(QSplitter* splitter, const QList<int>& from, const QList<int>& to,
                          int durationMs)
{
    if (!splitter || from.size() != to.size() || from.isEmpty()) {
        return;
    }

    if (QVariantAnimation* existing = splitter->findChild<QVariantAnimation*>(
            QLatin1String(kSplitterAnimProp), Qt::FindDirectChildrenOnly)) {
        existing->stop();
        existing->deleteLater();
    }

    if (!enabled()) {
        splitter->setSizes(to);
        return;
    }

    splitter->setSizes(from);

    auto* anim = new QVariantAnimation(splitter);
    anim->setObjectName(QLatin1String(kSplitterAnimProp));
    anim->setDuration(durationMs);
    anim->setEasingCurve(easeOut());
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    trackAnim(anim);

    QObject::connect(anim, &QVariantAnimation::valueChanged, splitter,
                     [splitter, from, to](const QVariant& v) {
                         const qreal t = v.toReal();
                         QList<int> sizes;
                         sizes.reserve(from.size());
                         for (int i = 0; i < from.size(); ++i) {
                             sizes.push_back(int(from[i] + (to[i] - from[i]) * t));
                         }
                         splitter->setSizes(sizes);
                     });
    QObject::connect(anim, &QVariantAnimation::finished, anim, &QObject::deleteLater);
    anim->start();
}

} // namespace Motion
