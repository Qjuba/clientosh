#include "platform/WindowStayOnTop.h"

#include <QWidget>
#include <QWindow>

#if defined(Q_OS_MACOS)
void clientoshSetMacStayOnTop(WId wid, bool on);
#endif

void setWindowStayOnTop(QWidget* widget, bool on)
{
    if (!widget) {
        return;
    }

    // Realize the native window so platform-specific APIs and QWindow flags apply.
    if (!widget->windowHandle()) {
        widget->winId();
    }

    if (QWindow* handle = widget->windowHandle()) {
        handle->setFlag(Qt::WindowStaysOnTopHint, on);
    } else {
        Qt::WindowFlags flags = widget->windowFlags();
        const Qt::WindowFlags before = flags;
        if (on) {
            flags |= Qt::WindowStaysOnTopHint;
        } else {
            flags &= ~Qt::WindowStaysOnTopHint;
        }
        if (flags != before) {
            const bool visible = widget->isVisible();
            widget->setWindowFlags(flags);
            if (visible) {
                widget->show();
            }
        }
    }

#if defined(Q_OS_MACOS)
    clientoshSetMacStayOnTop(widget->winId(), on);
#endif

    if (on && widget->isVisible()) {
        widget->raise();
    }
}
