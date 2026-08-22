#pragma once

#include <QString>

/** Ensure a visible console exists for CLI output (Windows). No-op elsewhere. */
void clientoshEnsureConsoleForCli();

/** Write UTF-8 text to the terminal (never to a Qt message box). */
void clientoshCliWrite(const char* utf8);

inline void clientoshCliWrite(const QString& text)
{
    const QByteArray bytes = text.toUtf8();
    clientoshCliWrite(bytes.constData());
}

