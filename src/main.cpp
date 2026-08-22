#include "MainWindow.h"
#include "PlatformFonts.h"
#include "core/AppSettings.h"
#include "core/CliLaunch.h"
#include "core/FontManager.h"
#include "ui/Motion.h"

#include <libssh/libssh.h>

#include <cstdio>

#include <QApplication>
#include <QIcon>
#include <QStyleFactory>
#include <QTimer>

#ifdef Q_OS_WIN
#include <winsock2.h>
#endif

int main(int argc, char* argv[])
{
#ifdef Q_OS_WIN
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    CliLaunch::Request cliRequest;
    bool cliConnect = false;
    bool verbose = false;

    if (argc > 1) {
        const int headlessCode =
            CliLaunch::runHeadlessPhase(argc, argv, &cliRequest, &cliConnect, &verbose);
        if (headlessCode >= 0) {
#ifdef Q_OS_WIN
            WSACleanup();
#endif
            return headlessCode;
        }
    }

    ssh_init();

    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("clientosh"));
    app.setOrganizationName(QStringLiteral("clientosh"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/terminal.svg")));

    if (verbose) {
        AppSettings::setSftpVerboseLogging(true);
    }

    FontManager::instance()->loadCachedFonts();
    app.setFont(clientoshUiFont(AppSettings::uiFontSize(), AppSettings::uiFontFamily()));

    Motion::loadFromSettings();

    MainWindow window;
    window.show();

    if (cliConnect) {
        const SessionProfile profile = cliRequest.profile;
        const bool withSftp = cliRequest.openSftpWithSsh;
        QTimer::singleShot(0, &window, [&window, profile, withSftp]() {
            window.launchFromCli(profile, withSftp);
        });
    }

    const int code = app.exec();

    ssh_finalize();

#ifdef Q_OS_WIN
    WSACleanup();
#endif
    return code;
}
