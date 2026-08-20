#pragma once

class QWidget;

/**
 * Request that `widget`'s top-level window stay above others.
 * Uses Qt hints on all platforms, plus a native AppKit level on macOS
 * where WindowStaysOnTopHint alone is unreliable.
 */
void setWindowStayOnTop(QWidget* widget, bool on);
