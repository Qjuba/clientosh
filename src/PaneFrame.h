#pragma once

#include "PanelTypes.h"

#include <QFrame>
#include <QPoint>

class QLabel;
class QToolButton;
class QVBoxLayout;
class QMimeData;
class DropOverlay;

class PaneFrame : public QFrame
{
    Q_OBJECT

public:
    explicit PaneFrame(const PanelRef& ref, QWidget* content, QWidget* parent = nullptr);

    PanelRef panelRef() const { return m_ref; }
    QWidget* content() const { return m_content; }
    QWidget* takeContent();
    void setContent(QWidget* content);
    void setTitle(const QString& title);
    QString title() const;
    void setActive(bool active);
    bool isActive() const { return m_active; }
    void releaseMotionResources();

    void showDropOverlay(bool on);
    void updateDropHover(const QPoint& localPos);
    DockEdge currentDropEdge() const;
    void clearDropHover();

signals:
    void focusRequested(const PanelRef& ref);
    void closeRequested(const PanelRef& ref);
    void panelDropRequested(const PanelRef& moving, DockEdge edge, const PanelRef& target);
    void headerDragFinished(); // after QDrag::exec — safe to mutate layout

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    bool acceptPanelMime(const QMimeData* mime, PanelRef* out = nullptr) const;
    void syncOverlayGeometry();
    void startHeaderDrag();
    bool isHeaderDragSource(QObject* obj) const;

    PanelRef m_ref;
    QWidget* m_content = nullptr;
    QWidget* m_header = nullptr;
    QLabel* m_title = nullptr;
    QToolButton* m_closeBtn = nullptr;
    QVBoxLayout* m_root = nullptr;
    DropOverlay* m_overlay = nullptr;
    bool m_active = false;
    bool m_headerPressing = false;
    QPoint m_headerDragStart;
};
