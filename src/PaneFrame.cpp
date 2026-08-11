#include "PaneFrame.h"
#include "DropOverlay.h"
#include "ui/Motion.h"

#include <QApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QToolButton>
#include <QVariantAnimation>
#include <QVBoxLayout>

namespace {
class PaneHeaderBar : public QWidget
{
public:
    explicit PaneHeaderBar(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setFixedHeight(22);
        setAttribute(Qt::WA_OpaquePaintEvent, true);
    }

    void setActive(bool on)
    {
        Motion::animateToward(this, &m_activeAmt, on ? 1.0 : 0.0, Motion::kNormalMs,
                              [this]() { update(); }, &m_anim);
    }

    void releaseMotionResources()
    {
        if (m_anim) {
            m_anim->stop();
            m_anim->deleteLater();
            m_anim = nullptr;
        }
        // Keep current visual state snapped (no mid-tween).
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        const int br = int(0x24 + (0x2e - 0x24) * m_activeAmt);
        const int bg = int(0x24 + (0x2e - 0x24) * m_activeAmt);
        const int bb = int(0x24 + (0x2e - 0x24) * m_activeAmt);
        p.fillRect(rect(), QColor(br, bg, bb));

        const int border = int(0x3a + (0x5a - 0x3a) * m_activeAmt);
        p.fillRect(0, height() - 1, width(), 1, QColor(border, border, border));

        if (m_activeAmt > 0.01) {
            p.fillRect(0, 0, 2, height(), QColor(0x7a, 0x7a, 0x7a, int(200 * m_activeAmt)));
        }
    }

private:
    qreal m_activeAmt = 0.0;
    QVariantAnimation* m_anim = nullptr;
};
} // namespace

PaneFrame::PaneFrame(const PanelRef& ref, QWidget* content, QWidget* parent)
    : QFrame(parent)
    , m_ref(ref)
    , m_content(content)
{
    setObjectName(QStringLiteral("paneFrame"));
    setAcceptDrops(true);
    setFrameShape(QFrame::NoFrame);

    m_root = new QVBoxLayout(this);
    m_root->setContentsMargins(0, 0, 0, 0);
    m_root->setSpacing(0);

    auto* header = new PaneHeaderBar(this);
    m_header = header;
    m_header->setObjectName(QStringLiteral("paneHeader"));
    m_header->setCursor(Qt::OpenHandCursor);
    m_header->setToolTip(QStringLiteral("drag to move pane"));
    auto* hLay = new QHBoxLayout(m_header);
    hLay->setContentsMargins(8, 0, 4, 0);
    hLay->setSpacing(4);

    m_title = new QLabel(m_header);
    m_title->setObjectName(QStringLiteral("paneTitle"));
    m_title->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_closeBtn = new Motion::HoverFillButton(m_header);
    m_closeBtn->setIcon(QIcon(QStringLiteral(":/icons/close.svg")));
    m_closeBtn->setIconSize(QSize(12, 12));
    m_closeBtn->setFixedSize(18, 18);
    m_closeBtn->setFocusPolicy(Qt::NoFocus);
    m_closeBtn->setObjectName(QStringLiteral("sessionChipClose"));
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setToolTip(QStringLiteral("close pane"));
    static_cast<Motion::HoverFillButton*>(m_closeBtn)->setHoverFill(QColor(0x50, 0x50, 0x50));

    hLay->addWidget(m_title, 1);
    hLay->addWidget(m_closeBtn);

    m_root->addWidget(m_header);
    if (m_content) {
        m_content->setParent(this);
        m_root->addWidget(m_content, 1);
    }

    m_overlay = new DropOverlay(this);
    m_overlay->hide();

    m_header->installEventFilter(this);

    connect(m_closeBtn, &QToolButton::clicked, this, [this]() { emit closeRequested(m_ref); });
    setActive(false);
}

QWidget* PaneFrame::takeContent()
{
    if (!m_content) {
        return nullptr;
    }
    m_root->removeWidget(m_content);
    m_content->setParent(nullptr);
    QWidget* w = m_content;
    m_content = nullptr;
    return w;
}

void PaneFrame::setContent(QWidget* content)
{
    if (m_content == content) {
        return;
    }
    if (m_content) {
        m_root->removeWidget(m_content);
        m_content->setParent(nullptr);
    }
    m_content = content;
    if (m_content) {
        m_content->setParent(this);
        m_root->addWidget(m_content, 1);
        m_content->show();
    }
}

void PaneFrame::setTitle(const QString& title)
{
    m_title->setText(title);
}

void PaneFrame::setActive(bool active)
{
    m_active = active;
    if (auto* header = dynamic_cast<PaneHeaderBar*>(m_header)) {
        header->setActive(active);
    }
    m_title->setStyleSheet(active ? QStringLiteral("color: #c8c8c8;")
                                  : QStringLiteral("color: #9a9a9a;"));
}

void PaneFrame::releaseMotionResources()
{
    if (auto* header = dynamic_cast<PaneHeaderBar*>(m_header)) {
        header->releaseMotionResources();
    }
    if (m_overlay) {
        m_overlay->releaseMotionResources();
    }
    if (auto* close = qobject_cast<Motion::HoverFillButton*>(m_closeBtn)) {
        close->releaseMotionResources();
    }
}

void PaneFrame::showDropOverlay(bool on)
{
    if (on) {
        syncOverlayGeometry();
        m_overlay->setShown(true);
    } else {
        m_overlay->setShown(false);
    }
}

void PaneFrame::updateDropHover(const QPoint& localPos)
{
    if (!m_overlay->isVisible()) {
        showDropOverlay(true);
    }
    const QPoint overlayPos = m_overlay->mapFrom(this, localPos);
    m_overlay->setHoveredEdge(m_overlay->edgeAt(overlayPos));
}

DockEdge PaneFrame::currentDropEdge() const
{
    return m_overlay->hoveredEdge();
}

void PaneFrame::clearDropHover()
{
    showDropOverlay(false);
}

bool PaneFrame::isHeaderDragSource(QObject* obj) const
{
    return obj == m_header;
}

void PaneFrame::startHeaderDrag()
{
    m_headerPressing = false;
    m_header->setCursor(Qt::OpenHandCursor);
    emit focusRequested(m_ref);

    auto* mime = new QMimeData;
    mime->setData(QLatin1String(kClientoshPanelMime), m_ref.toMime());
    if (m_ref.kind == PanelKind::Terminal) {
        mime->setData(QLatin1String(kClientoshSessionMime), m_ref.sessionId.toUtf8());
    }

    auto* drag = new QDrag(this);
    drag->setMimeData(mime);
    drag->setPixmap(m_header->grab());
    drag->setHotSpot(QPoint(qMax(0, m_header->width() / 2), qMax(0, m_header->height() / 2)));
    drag->exec(Qt::MoveAction);
    emit headerDragFinished();
}

bool PaneFrame::eventFilter(QObject* watched, QEvent* event)
{
    if (!isHeaderDragSource(watched)) {
        return QFrame::eventFilter(watched, event);
    }

    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            if (m_closeBtn->geometry().contains(me->pos())) {
                m_headerPressing = false;
                return false;
            }
            m_headerPressing = true;
            m_headerDragStart = me->pos();
            m_header->setCursor(Qt::ClosedHandCursor);
            emit focusRequested(m_ref);
            return true;
        }
        break;
    }
    case QEvent::MouseMove: {
        auto* me = static_cast<QMouseEvent*>(event);
        if (m_headerPressing && (me->buttons() & Qt::LeftButton)) {
            if ((me->pos() - m_headerDragStart).manhattanLength() >= QApplication::startDragDistance()) {
                startHeaderDrag();
                return true;
            }
        }
        break;
    }
    case QEvent::MouseButtonRelease: {
        if (m_headerPressing) {
            m_headerPressing = false;
            m_header->setCursor(Qt::OpenHandCursor);
            return true;
        }
        break;
    }
    default:
        break;
    }
    return QFrame::eventFilter(watched, event);
}

bool PaneFrame::acceptPanelMime(const QMimeData* mime, PanelRef* out) const
{
    if (!mime) {
        return false;
    }
    PanelRef ref;
    if (mime->hasFormat(QLatin1String(kClientoshPanelMime))) {
        ref = PanelRef::fromMime(mime->data(QLatin1String(kClientoshPanelMime)));
    } else if (mime->hasFormat(QLatin1String(kClientoshSessionMime))) {
        ref = PanelRef::terminal(QString::fromUtf8(mime->data(QLatin1String(kClientoshSessionMime))));
    }
    if (!ref.isValid() || ref == m_ref) {
        return false;
    }
    if (out) {
        *out = ref;
    }
    return true;
}

void PaneFrame::dragEnterEvent(QDragEnterEvent* event)
{
    if (acceptPanelMime(event->mimeData())) {
        showDropOverlay(true);
        updateDropHover(event->position().toPoint());
        event->acceptProposedAction();
        return;
    }
    event->ignore();
}

void PaneFrame::dragMoveEvent(QDragMoveEvent* event)
{
    if (acceptPanelMime(event->mimeData())) {
        updateDropHover(event->position().toPoint());
        event->acceptProposedAction();
        return;
    }
    event->ignore();
}

void PaneFrame::dragLeaveEvent(QDragLeaveEvent* event)
{
    clearDropHover();
    QFrame::dragLeaveEvent(event);
}

void PaneFrame::dropEvent(QDropEvent* event)
{
    PanelRef moving;
    if (!acceptPanelMime(event->mimeData(), &moving)) {
        clearDropHover();
        event->ignore();
        return;
    }
    const QPoint overlayPos = m_overlay->mapFrom(this, event->position().toPoint());
    const DockEdge edge = m_overlay->edgeAt(overlayPos);
    clearDropHover();
    if (edge == DockEdge::None) {
        event->ignore();
        return;
    }
    emit panelDropRequested(moving, edge, m_ref);
    event->acceptProposedAction();
}

void PaneFrame::resizeEvent(QResizeEvent* event)
{
    QFrame::resizeEvent(event);
    syncOverlayGeometry();
}

void PaneFrame::syncOverlayGeometry()
{
    if (!m_overlay) {
        return;
    }
    m_overlay->setGeometry(rect());
}
