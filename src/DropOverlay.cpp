#include "DropOverlay.h"
#include "ui/Motion.h"

#include <QPainter>
#include <QPaintEvent>
#include <QVariantAnimation>

DropOverlay::DropOverlay(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    hide();
}

void DropOverlay::setFade(qreal value)
{
    const qreal v = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(m_fade, v)) {
        return;
    }
    m_fade = v;
    update();
}

void DropOverlay::setHighlight(qreal value)
{
    const qreal v = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(m_highlight, v)) {
        return;
    }
    m_highlight = v;
    update();
}

void DropOverlay::setShown(bool on)
{
    if (on) {
        if (!isVisible()) {
            m_fade = 0.0;
            show();
        }
        raise();
        Motion::animateToward(this, &m_fade, 1.0, Motion::kFastMs, [this]() { update(); }, &m_fadeAnim);
        return;
    }

    if (!isVisible()) {
        return;
    }

    Motion::animateToward(
        this, &m_fade, 0.0, Motion::kFastMs,
        [this]() {
            update();
            if (m_fade <= 0.01) {
                hide();
                setHoveredEdge(DockEdge::None);
            }
        },
        &m_fadeAnim);
}

void DropOverlay::releaseMotionResources()
{
    if (m_fadeAnim) {
        m_fadeAnim->stop();
        m_fadeAnim->deleteLater();
        m_fadeAnim = nullptr;
    }
    if (m_highlightAnim) {
        m_highlightAnim->stop();
        m_highlightAnim->deleteLater();
        m_highlightAnim = nullptr;
    }
    if (m_fade > 0.01 && isVisible()) {
        m_fade = 1.0;
        m_highlight = (m_edge == DockEdge::None) ? 0.0 : 1.0;
        update();
    } else {
        m_fade = 0.0;
        m_highlight = 0.0;
        hide();
        m_edge = DockEdge::None;
    }
}

QRect DropOverlay::zoneRect(DockEdge edge) const
{
    const int w = width();
    const int h = height();
    const int edgeW = qMax(40, w / 4);
    const int edgeH = qMax(40, h / 4);

    switch (edge) {
    case DockEdge::Left:
        return QRect(0, 0, edgeW, h);
    case DockEdge::Right:
        return QRect(w - edgeW, 0, edgeW, h);
    case DockEdge::Top:
        return QRect(0, 0, w, edgeH);
    case DockEdge::Bottom:
        return QRect(0, h - edgeH, w, edgeH);
    default:
        return {};
    }
}

DockEdge DropOverlay::edgeAt(const QPoint& localPos) const
{
    const QRect left = zoneRect(DockEdge::Left);
    const QRect right = zoneRect(DockEdge::Right);
    const QRect top = zoneRect(DockEdge::Top);
    const QRect bottom = zoneRect(DockEdge::Bottom);

    struct Cand {
        DockEdge edge;
        QRect rect;
    };
    const Cand cands[] = {
        {DockEdge::Left, left},
        {DockEdge::Right, right},
        {DockEdge::Top, top},
        {DockEdge::Bottom, bottom},
    };

    DockEdge best = DockEdge::None;
    qreal bestDist = 1e12;
    for (const Cand& c : cands) {
        if (!c.rect.contains(localPos)) {
            continue;
        }
        const qreal d = QPointF(localPos - c.rect.center()).manhattanLength();
        if (d < bestDist) {
            bestDist = d;
            best = c.edge;
        }
    }

    if (best != DockEdge::None) {
        return best;
    }

    const qreal nx = localPos.x() / qMax(1.0, double(width()));
    const qreal ny = localPos.y() / qMax(1.0, double(height()));
    const qreal dl = nx;
    const qreal dr = 1.0 - nx;
    const qreal dt = ny;
    const qreal db = 1.0 - ny;
    const qreal m = qMin(qMin(dl, dr), qMin(dt, db));
    if (m == dl) {
        return DockEdge::Left;
    }
    if (m == dr) {
        return DockEdge::Right;
    }
    if (m == dt) {
        return DockEdge::Top;
    }
    return DockEdge::Bottom;
}

void DropOverlay::setHoveredEdge(DockEdge edge)
{
    if (m_edge == edge) {
        return;
    }
    m_edge = edge;
    if (edge == DockEdge::None) {
        Motion::animateToward(this, &m_highlight, 0.0, Motion::kFastMs, [this]() { update(); },
                              &m_highlightAnim);
    } else {
        m_highlight = 0.35;
        Motion::animateToward(this, &m_highlight, 1.0, Motion::kFastMs, [this]() { update(); },
                              &m_highlightAnim);
    }
    update();
}

void DropOverlay::paintEvent(QPaintEvent*)
{
    if (m_fade <= 0.01) {
        return;
    }

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setOpacity(m_fade);
    p.fillRect(rect(), QColor(0, 0, 0, 45));

    auto drawZone = [&](DockEdge edge, const QColor& fill, const QString& label = {}) {
        const QRect r = zoneRect(edge);
        if (!r.isValid()) {
            return;
        }
        if (edge == m_edge) {
            QColor hot = fill;
            hot.setAlpha(int(fill.alpha() * (0.55 + 0.45 * m_highlight)));
            p.fillRect(r, hot);
            p.setPen(QPen(QColor(0xb0, 0xb0, 0xb0, int(180 + 75 * m_highlight)), 2));
            p.drawRect(r.adjusted(1, 1, -2, -2));
            if (!label.isEmpty()) {
                p.setPen(QColor(0xd0, 0xd0, 0xd0));
                p.drawText(r, Qt::AlignCenter, label);
            }
        } else {
            p.fillRect(r, QColor(255, 255, 255, 16));
            if (!label.isEmpty()) {
                p.setPen(QColor(0x80, 0x80, 0x80));
                p.drawText(r, Qt::AlignCenter, label);
            }
        }
    };

    drawZone(DockEdge::Left, QColor(0x9a, 0x9a, 0x9a, 95));
    drawZone(DockEdge::Right, QColor(0x9a, 0x9a, 0x9a, 95));
    drawZone(DockEdge::Top, QColor(0x9a, 0x9a, 0x9a, 95));
    drawZone(DockEdge::Bottom, QColor(0x9a, 0x9a, 0x9a, 95));
}
