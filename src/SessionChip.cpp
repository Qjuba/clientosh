#include "SessionChip.h"
#include "ui/Motion.h"

#include <QApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEnterEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QToolButton>
#include <QVariantAnimation>

SessionChip::SessionChip(const PanelRef& ref, const QString& title, bool active, QWidget* parent)
    : QFrame(parent)
    , m_ref(ref)
    , m_active(active)
    , m_activeAmt(active ? 1.0 : 0.0)
{
    setObjectName(QStringLiteral("sessionChip"));
    setAcceptDrops(true);
    setCursor(Qt::OpenHandCursor);
    setAttribute(Qt::WA_Hover, true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);

    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(6, 1, 2, 1);
    lay->setSpacing(2);

    m_labelBtn = new QPushButton(title, this);
    m_labelBtn->setObjectName(QStringLiteral("sessionChipBtn"));
    m_labelBtn->setFocusPolicy(Qt::NoFocus);
    m_labelBtn->setFlat(true);
    m_labelBtn->setAttribute(Qt::WA_TransparentForMouseEvents);

    auto* closeBtn = new Motion::HoverFillButton(this);
    closeBtn->setIcon(QIcon(QStringLiteral(":/icons/close.svg")));
    closeBtn->setIconSize(QSize(12, 12));
    closeBtn->setFixedSize(18, 18);
    closeBtn->setFocusPolicy(Qt::NoFocus);
    closeBtn->setObjectName(QStringLiteral("sessionChipClose"));
    closeBtn->setToolTip(QStringLiteral("close"));
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setHoverFill(QColor(0x50, 0x50, 0x50));
    m_closeBtn = closeBtn;

    lay->addWidget(m_labelBtn);
    lay->addWidget(m_closeBtn);

    connect(m_closeBtn, &QToolButton::clicked, this, [this]() { emit closeRequested(m_ref); });
}

void SessionChip::setHover(qreal value)
{
    const qreal v = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(m_hover, v)) {
        return;
    }
    m_hover = v;
    update();
}

void SessionChip::setActiveAmt(qreal value)
{
    const qreal v = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(m_activeAmt, v)) {
        return;
    }
    m_activeAmt = v;
    update();
}

void SessionChip::animateHover(qreal target)
{
    Motion::animateToward(this, &m_hover, target, Motion::kFastMs, [this]() { update(); }, &m_hoverAnim);
}

void SessionChip::animateActive(qreal target)
{
    Motion::animateToward(this, &m_activeAmt, target, Motion::kNormalMs, [this]() { update(); },
                          &m_activeAnim);
}

void SessionChip::setActive(bool active)
{
    if (m_active == active) {
        return;
    }
    m_active = active;
    animateActive(active ? 1.0 : 0.0);
}

void SessionChip::releaseMotionResources()
{
    if (m_hoverAnim) {
        m_hoverAnim->stop();
        m_hoverAnim->deleteLater();
        m_hoverAnim = nullptr;
    }
    if (m_activeAnim) {
        m_activeAnim->stop();
        m_activeAnim->deleteLater();
        m_activeAnim = nullptr;
    }
    // Snap to logical endpoints so paint doesn't hold mid-frame state.
    m_hover = 0.0;
    m_activeAmt = m_active ? 1.0 : 0.0;
    update();
}

void SessionChip::enterEvent(QEnterEvent* event)
{
    animateHover(1.0);
    QFrame::enterEvent(event);
}

void SessionChip::leaveEvent(QEvent* event)
{
    animateHover(0.0);
    QFrame::leaveEvent(event);
}

void SessionChip::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    const qreal mix = qMax(m_activeAmt, m_hover * 0.55);
    const int r = int(0x1e + (0x33 - 0x1e) * mix);
    const int g = int(0x1e + (0x33 - 0x1e) * mix);
    const int b = int(0x1e + (0x33 - 0x1e) * mix);
    p.fillRect(rect(), QColor(r, g, b));

    const int border = int(0x3a + (0x6a - 0x3a) * m_activeAmt);
    p.setPen(QColor(border, border, border));
    p.drawRect(rect().adjusted(0, 0, -1, -1));

    // Active selection underline — grows in from center.
    if (m_activeAmt > 0.01) {
        const int underlineW = int((width() - 4) * m_activeAmt);
        const int x = (width() - underlineW) / 2;
        p.fillRect(x, height() - 2, underlineW, 2, QColor(0x8a, 0x8a, 0x8a, int(220 * m_activeAmt)));
    }
}

void SessionChip::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && !m_closeBtn->geometry().contains(event->pos())) {
        m_pressing = true;
        m_dragStart = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QFrame::mousePressEvent(event);
}

void SessionChip::mouseMoveEvent(QMouseEvent* event)
{
    if (m_pressing && (event->buttons() & Qt::LeftButton)) {
        if ((event->pos() - m_dragStart).manhattanLength() >= QApplication::startDragDistance()) {
            m_pressing = false;
            startDrag();
            return;
        }
    }
    QFrame::mouseMoveEvent(event);
}

void SessionChip::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_pressing && event->button() == Qt::LeftButton) {
        m_pressing = false;
        setCursor(Qt::OpenHandCursor);
        emit activated(m_ref);
        event->accept();
        return;
    }
    QFrame::mouseReleaseEvent(event);
}

PanelRef SessionChip::panelFromMime(const QMimeData* mime) const
{
    if (!mime) {
        return {};
    }
    if (mime->hasFormat(QLatin1String(kClientoshPanelMime))) {
        return PanelRef::fromMime(mime->data(QLatin1String(kClientoshPanelMime)));
    }
    if (mime->hasFormat(QLatin1String(kClientoshSessionMime))) {
        return PanelRef::terminal(QString::fromUtf8(mime->data(QLatin1String(kClientoshSessionMime))));
    }
    return {};
}

void SessionChip::startDrag()
{
    setCursor(Qt::OpenHandCursor);

    auto* mime = new QMimeData;
    mime->setData(QLatin1String(kClientoshPanelMime), m_ref.toMime());
    if (m_ref.kind == PanelKind::Terminal) {
        mime->setData(QLatin1String(kClientoshSessionMime), m_ref.sessionId.toUtf8());
    }

    auto* drag = new QDrag(this);
    drag->setMimeData(mime);
    drag->setPixmap(grab());
    drag->setHotSpot(QPoint(qMax(0, width() / 2), qMax(0, height() / 2)));
    drag->exec(Qt::MoveAction);
    emit dragFinished();
}

void SessionChip::dragEnterEvent(QDragEnterEvent* event)
{
    const PanelRef other = panelFromMime(event->mimeData());
    if (other.isValid() && other != m_ref) {
        emit hoverActivated(m_ref);
        event->acceptProposedAction();
        return;
    }
    event->ignore();
}

void SessionChip::dragMoveEvent(QDragMoveEvent* event)
{
    const PanelRef other = panelFromMime(event->mimeData());
    if (other.isValid() && other != m_ref) {
        event->acceptProposedAction();
        return;
    }
    event->ignore();
}

void SessionChip::dropEvent(QDropEvent* event)
{
    const PanelRef from = panelFromMime(event->mimeData());
    if (!from.isValid() || from == m_ref) {
        event->ignore();
        return;
    }
    emit reorderRequested(from, m_ref);
    event->acceptProposedAction();
}
