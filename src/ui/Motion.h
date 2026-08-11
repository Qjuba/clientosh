#pragma once

#include <QEasingCurve>
#include <QList>
#include <QToolButton>
#include <QVariantAnimation>
#include <QWidget>

#include <functional>

class QSplitter;

/**
 * Lightweight UI motion helpers for the Widgets shell.
 * Uses Qt's animation timer (vsync-paced) and paints only small chrome —
 * no continuous timers while idle, no full-window effects.
 *
 * When disabled via setEnabled(false), all transitions snap instantly and
 * animation objects / pixmap cache entries are released.
 */
namespace Motion {

constexpr int kFastMs = 120;
constexpr int kNormalMs = 160;
constexpr int kSlowMs = 200;

inline QEasingCurve easeOut()
{
    return QEasingCurve(QEasingCurve::OutCubic);
}

bool enabled();
void setEnabled(bool on);
void loadFromSettings();
/** Stop running animations, delete motion objects, clear pixmap cache. */
void releaseCaches();

/** Soft fill hover for icon / chrome buttons (120ms). */
class HoverFillButton : public QToolButton
{
    Q_OBJECT
    Q_PROPERTY(qreal hover READ hover WRITE setHover)

public:
    explicit HoverFillButton(QWidget* parent = nullptr);

    qreal hover() const { return m_hover; }
    void setHover(qreal value);

    void setHoverFill(const QColor& color);
    void releaseMotionResources();

protected:
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void animateHover(qreal target);

    qreal m_hover = 0.0;
    QColor m_fill{60, 60, 60, 255};
    QVariantAnimation* m_anim = nullptr;
};

/** Animate a qreal toward target; invokes onTick each frame (typically update()). */
void animateToward(QObject* owner, qreal* value, qreal target, int durationMs,
                   const std::function<void()>& onTick, QVariantAnimation** animSlot);

/** Equalize / morph splitter sizes after a dock (not during live handle drag). */
void animateSplitterSizes(QSplitter* splitter, const QList<int>& from, const QList<int>& to,
                          int durationMs = kNormalMs);

} // namespace Motion
