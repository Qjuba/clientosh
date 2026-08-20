#include <QtGui/qwindowdefs.h>

#import <AppKit/AppKit.h>

void clientoshSetMacStayOnTop(WId wid, bool on)
{
    if (!wid) {
        return;
    }

    NSView* view = (__bridge NSView*)(void*)wid;
    if (!view) {
        return;
    }

    NSWindow* window = [view window];
    if (!window) {
        return;
    }

    // NSFloatingWindowLevel keeps the window above normal app windows.
    // NSNormalWindowLevel restores standard stacking.
    [window setLevel:(on ? NSFloatingWindowLevel : NSNormalWindowLevel)];
}
