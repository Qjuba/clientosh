#pragma once

#include "PanelTypes.h"

#include <QFrame>
#include <QString>

class QToolButton;
class QPushButton;
class QMimeData;
class QVariantAnimation;
class QContextMenuEvent;

class SessionChip : public QFrame
{
    Q_OBJECT
    Q_PROPERTY(qreal hover READ hover WRITE setHover)
    Q_PROPERTY(qreal activeAmt READ activeAmt WRITE setActiveAmt)

public:
    explicit SessionChip(const PanelRef& ref, const QString& title, bool active,
                         QWidget* parent = nullptr);

    PanelRef panelRef() const { return m_ref; }
    QString sessionId() const { return m_ref.sessionId; }
    void setActive(bool active);
    void releaseMotionResources();

    qreal hover() const { return m_hover; }
    void setHover(qreal value);
    qreal activeAmt() const { return m_activeAmt; }
    void setActiveAmt(qreal value);

signals:
    void activated(const PanelRef& ref);
    void closeRequested(const PanelRef& ref);
    void reorderRequested(const PanelRef& from, const PanelRef& before);
    void hoverActivated(const PanelRef& ref); // drag-hover over this tab → show viewport
    void contextMenuRequested(const PanelRef& ref, const QPoint& globalPos);
    void dragFinished(); // after QDrag::exec — safe to mutate layout / flush docks

protected:
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void startDrag();
    PanelRef panelFromMime(const QMimeData* mime) const;
    void animateHover(qreal target);
    void animateActive(qreal target);

    PanelRef m_ref;
    QPushButton* m_labelBtn = nullptr;
    QToolButton* m_closeBtn = nullptr;
    QPoint m_dragStart;
    bool m_pressing = false;
    bool m_active = false;
    qreal m_hover = 0.0;
    qreal m_activeAmt = 0.0;
    QVariantAnimation* m_hoverAnim = nullptr;
    QVariantAnimation* m_activeAnim = nullptr;
};
