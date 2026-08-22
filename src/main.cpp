#include "MainWindow.h"
#include "PlatformFonts.h"
#include "core/AppSettings.h"
#include "core/FontManager.h"
#include "ui/Motion.h"

#include <libssh/libssh.h>

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QStyleFactory>
#include <QTimer>

#ifdef Q_OS_WIN
#include <winsock2.h>
#endif

namespace {

bool parseEndpoint(const QString& value, QString* hostOut, int* portOut)
{
    const QString endpoint = value.trimmed();
    QString host;
    QString portText;

    if (endpoint.startsWith(QLatin1Char('['))) {
        const qsizetype closingBracket = endpoint.indexOf(QLatin1Char(']'));
        if (closingBracket <= 1 || closingBracket + 1 >= endpoint.size()
            || endpoint.at(closingBracket + 1) != QLatin1Char(':')) {
            return false;
        }
        host = endpoint.mid(1, closingBracket - 1);
        portText = endpoint.mid(closingBracket + 2);
    } else {
        const qsizetype colon = endpoint.lastIndexOf(QLatin1Char(':'));
        if (colon <= 0 || colon + 1 >= endpoint.size()) {
            return false;
        }
        host = endpoint.left(colon);
        portText = endpoint.mid(colon + 1);
    }

    bool portOk = false;
    const int port = portText.toInt(&portOk);
    if (host.trimmed().isEmpty() || !portOk || port < 1 || port > 65535) {
        return false;
    }

    *hostOut = host.trimmed();
    *portOut = port;
    return true;
}

} // namespace

int main(int argc, char* argv[])
{
#ifdef Q_OS_WIN
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    ssh_init();

    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("clientosh"));
    app.setOrganizationName(QStringLiteral("clientosh"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/terminal.svg")));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("clientosh - SSH, SFTP, and Telnet client"));
    // Accept the requested `-name` spelling in addition to conventional `--name`.
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption verboseOpt({QStringLiteral("v"), QStringLiteral("verbose")},
                                  QStringLiteral("Enable verbose SFTP debug logging (also: Settings > SFTP)."));
    parser.addOption(verboseOpt);
    QCommandLineOption nameOpt({QStringLiteral("n"), QStringLiteral("name")},
                               QStringLiteral("Set the title of the opened terminal tab."),
                               QStringLiteral("tab_name"));
    parser.addOption(nameOpt);
    parser.addPositionalArgument(QStringLiteral("protocol"),
                                 QStringLiteral("Connection protocol: ssh or telnet."),
                                 QStringLiteral("[ssh|telnet]"));
    parser.addPositionalArgument(QStringLiteral("endpoint"),
                                 QStringLiteral("Host and port (host:port or [IPv6]:port)."),
                                 QStringLiteral("[host:port]"));
    parser.process(app);
    if (parser.isSet(verboseOpt)) {
        AppSettings::setSftpVerboseLogging(true);
    }

    SessionProfile commandLineProfile;
    bool hasCommandLineSession = false;
    const QStringList positional = parser.positionalArguments();
    if (!positional.isEmpty()) {
        if (positional.size() != 2) {
            parser.showHelp(1);
        }

        const QString protocol = positional.at(0).toLower();
        if (protocol != QLatin1String("ssh") && protocol != QLatin1String("telnet")) {
            parser.showHelp(1);
        }

        QString host;
        int port = 0;
        if (!parseEndpoint(positional.at(1), &host, &port)) {
            parser.showHelp(1);
        }

        const ConnectionMode mode = protocol == QLatin1String("telnet")
            ? ConnectionMode::Telnet
            : ConnectionMode::Ssh;

        // Reuse credentials and key settings when the endpoint already exists
        // as a saved profile. Otherwise create a temporary ad-hoc profile.
        const QVector<SessionProfile> savedProfiles = loadProfiles();
        for (const SessionProfile& saved : savedProfiles) {
            if (saved.host.compare(host, Qt::CaseInsensitive) == 0
                && saved.port == port && saved.connectionMode == mode) {
                commandLineProfile = saved;
                break;
            }
        }
        commandLineProfile.host = host;
        commandLineProfile.port = port;
        commandLineProfile.connectionMode = mode;
        if (commandLineProfile.user.trimmed().isEmpty()) {
            commandLineProfile.user = AppSettings::defaultUser();
        }
        if (parser.isSet(nameOpt)) {
            commandLineProfile.name = parser.value(nameOpt).trimmed();
        }
        hasCommandLineSession = true;
    }

    FontManager::instance()->loadCachedFonts();
    app.setFont(clientoshUiFont(AppSettings::uiFontSize(), AppSettings::uiFontFamily()));

    Motion::loadFromSettings();

    MainWindow window;
    window.show();
    if (hasCommandLineSession) {
        QTimer::singleShot(0, &window, [&window, commandLineProfile]() {
            window.openSession(commandLineProfile);
        });
    }

    const int code = app.exec();

    ssh_finalize();

#ifdef Q_OS_WIN
    WSACleanup();
#endif
    return code;
}
