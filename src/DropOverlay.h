#pragma once

#include "PanelTypes.h"

#include <QWidget>

class QVariantAnimation;

class DropOverlay : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal fade READ fade WRITE setFade)
    Q_PROPERTY(qreal highlight READ highlight WRITE setHighlight)

public:
    explicit DropOverlay(QWidget* parent = nullptr);

    void setHoveredEdge(DockEdge edge);
    DockEdge edgeAt(const QPoint& localPos) const;
    DockEdge hoveredEdge() const { return m_edge; }

    void setShown(bool on);
    void releaseMotionResources();

    qreal fade() const { return m_fade; }
    void setFade(qreal value);
    qreal highlight() const { return m_highlight; }
    void setHighlight(qreal value);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QRect zoneRect(DockEdge edge) const;

    DockEdge m_edge = DockEdge::None;
    qreal m_fade = 0.0;
    qreal m_highlight = 0.0;
    QVariantAnimation* m_fadeAnim = nullptr;
    QVariantAnimation* m_highlightAnim = nullptr;
};
