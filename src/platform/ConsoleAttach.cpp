#include "ConsoleAttach.h"

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

#include <cstdio>
#include <string>

void clientoshEnsureConsoleForCli()
{
#ifdef Q_OS_WIN
    if (GetConsoleWindow() == nullptr) {
        if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
            AllocConsole();
        }
    }
    // Never freopen stdout here — freopen("CONOUT$") breaks shell redirection (> file).
#endif
}

bool clientoshStdoutIsRedirected()
{
#ifdef Q_OS_WIN
    const HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == nullptr || hOut == INVALID_HANDLE_VALUE) {
        return false;
    }
    const DWORD fileType = GetFileType(hOut);
    return fileType == FILE_TYPE_PIPE || fileType == FILE_TYPE_DISK;
#else
    return false;
#endif
}

void clientoshCliWrite(const char* utf8)
{
    if (!utf8 || utf8[0] == '\0') {
        return;
    }

#ifdef Q_OS_WIN
    clientoshEnsureConsoleForCli();

    if (!clientoshStdoutIsRedirected() && GetConsoleWindow() != nullptr) {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != nullptr && hOut != INVALID_HANDLE_VALUE) {
            const QString text = QString::fromUtf8(utf8);
            const std::wstring wide = text.toStdWString();
            DWORD written = 0;
            if (!wide.empty()) {
                WriteConsoleW(hOut, wide.data(), static_cast<DWORD>(wide.size()), &written, nullptr);
                return;
            }
        }
    }
#endif

    std::fputs(utf8, stdout);
    std::fflush(stdout);
}
