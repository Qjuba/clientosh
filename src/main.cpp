#include "MainWindow.h"
#include "PlatformFonts.h"
#include "core/AppSettings.h"
#include "core/FontManager.h"
#include "ui/Motion.h"

#include <libssh/libssh.h>

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>

#ifdef Q_OS_WIN
#include <winsock2.h>
#endif

int main(int argc, char* argv[])
{
#ifdef Q_OS_WIN
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    ssh_init();

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("clientosh"));
    app.setOrganizationName(QStringLiteral("clientosh"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/terminal.svg")));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("clientosh — ssh client"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption verboseOpt({QStringLiteral("v"), QStringLiteral("verbose")},
                                  QStringLiteral("Enable verbose SFTP debug logging (also: Settings > SFTP)."));
    parser.addOption(verboseOpt);
    parser.process(app);
    if (parser.isSet(verboseOpt)) {
        AppSettings::setSftpVerboseLogging(true);
    }

    FontManager::instance()->loadCachedFonts();
    app.setFont(clientoshUiFont(AppSettings::uiFontSize(), AppSettings::uiFontFamily()));

    Motion::loadFromSettings();

    MainWindow window;
    window.show();

    const int code = app.exec();

    ssh_finalize();

#ifdef Q_OS_WIN
    WSACleanup();
#endif
    return code;
}
