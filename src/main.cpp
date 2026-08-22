#include "MainWindow.h"
#include "PlatformFonts.h"
#include "core/AppSettings.h"
#include "core/FontManager.h"
#include "ui/Motion.h"

#include <libssh/libssh.h>

#include <QApplication>
#include <QCommandLineParser>
#include <QCryptographicHash>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStandardPaths>
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

QString commandServerName()
{
    const QByteArray home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation).toUtf8();
    const QByteArray userKey = QCryptographicHash::hash(home, QCryptographicHash::Sha256).toHex().left(12);
    return QStringLiteral("clientosh-%1").arg(QString::fromLatin1(userKey));
}

bool forwardCommand(const QJsonObject& request)
{
    if (request.isEmpty()) {
        return false;
    }

    QLocalSocket socket;
    socket.connectToServer(commandServerName(), QIODevice::WriteOnly);
    if (!socket.waitForConnected(500)) {
        return false;
    }

    QByteArray payload = QJsonDocument(request).toJson(QJsonDocument::Compact);
    payload.append('\n');
    if (socket.write(payload) != payload.size() || !socket.waitForBytesWritten(1000)) {
        return false;
    }
    socket.disconnectFromServer();
    return true;
}

SessionProfile profileFromCommand(const QJsonObject& request)
{
    SessionProfile profile;
    const QString host = request.value(QStringLiteral("host")).toString().trimmed();
    const int port = request.value(QStringLiteral("port")).toInt();
    const ConnectionMode mode = connectionModeFromString(
        request.value(QStringLiteral("protocol")).toString());
    const bool serial = mode == ConnectionMode::Serial;
    if (host.isEmpty() || (!serial && (port < 1 || port > 65535))) {
        return profile;
    }

    // Reuse credentials and key settings when the endpoint already exists
    // as a saved profile. Otherwise create a temporary ad-hoc profile.
    const QVector<SessionProfile> savedProfiles = loadProfiles();
    for (const SessionProfile& saved : savedProfiles) {
        if (saved.host.compare(host, Qt::CaseInsensitive) == 0
            && (serial || saved.port == port) && saved.connectionMode == mode) {
            profile = saved;
            break;
        }
    }
    profile.host = host;
    profile.port = port;
    profile.connectionMode = mode;
    if (serial) {
        profile.serialBaudRate = request.value(QStringLiteral("baud")).toInt(115200);
        profile.system = QStringLiteral("Serial");
    } else if (profile.user.trimmed().isEmpty()) {
        profile.user = AppSettings::defaultUser();
    }
    if (request.value(QStringLiteral("hasName")).toBool()) {
        profile.name = request.value(QStringLiteral("name")).toString().trimmed();
    }
    return profile;
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
    parser.setApplicationDescription(QStringLiteral("clientosh - SSH, SFTP, Telnet, and serial client"));
    // Accept the requested `-name` spelling in addition to conventional `--name`.
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption verboseOpt(QStringLiteral("verbose"),
                                  QStringLiteral("Enable verbose SFTP debug logging (also: Settings > SFTP)."));
    parser.addOption(verboseOpt);
    QCommandLineOption nameOpt({QStringLiteral("n"), QStringLiteral("name")},
                               QStringLiteral("Set the title of the opened terminal tab."),
                               QStringLiteral("tab_name"));
    parser.addOption(nameOpt);
    QCommandLineOption baudOpt(QStringLiteral("baud"),
                               QStringLiteral("Serial baud rate (default: 115200)."),
                               QStringLiteral("rate"), QStringLiteral("115200"));
    parser.addOption(baudOpt);
    parser.addPositionalArgument(QStringLiteral("protocol"),
                                 QStringLiteral("Connection protocol: ssh, telnet, or serial."),
                                 QStringLiteral("[ssh|telnet|serial]"));
    parser.addPositionalArgument(QStringLiteral("endpoint"),
                                 QStringLiteral("Host:port, or a serial device such as COM3 or /dev/ttyUSB0."),
                                 QStringLiteral("[endpoint]"));
    parser.process(app);
    if (parser.isSet(verboseOpt)) {
        AppSettings::setSftpVerboseLogging(true);
    }

    QJsonObject commandLineRequest;
    const QStringList positional = parser.positionalArguments();
    if (!positional.isEmpty()) {
        if (positional.size() != 2) {
            parser.showHelp(1);
        }

        const QString protocol = positional.at(0).toLower();
        const bool serial = protocol == QLatin1String("serial");
        if (protocol != QLatin1String("ssh") && protocol != QLatin1String("telnet") && !serial) {
            parser.showHelp(1);
        }

        QString host;
        int port = 0;
        if (serial) {
            host = positional.at(1).trimmed();
            bool baudOk = false;
            const int baud = parser.value(baudOpt).toInt(&baudOk);
            if (host.isEmpty() || !baudOk || baud <= 0) parser.showHelp(1);
            commandLineRequest.insert(QStringLiteral("baud"), baud);
        } else {
            if (!parseEndpoint(positional.at(1), &host, &port)) parser.showHelp(1);
        }

        commandLineRequest.insert(QStringLiteral("protocol"), protocol);
        commandLineRequest.insert(QStringLiteral("host"), host);
        commandLineRequest.insert(QStringLiteral("port"), port);
        commandLineRequest.insert(QStringLiteral("hasName"), parser.isSet(nameOpt));
        commandLineRequest.insert(QStringLiteral("name"), parser.value(nameOpt).trimmed());
    }

    // A later CLI invocation only sends its connection request to the already
    // running process. It must not create another top-level window.
    if (forwardCommand(commandLineRequest)) {
        ssh_finalize();
#ifdef Q_OS_WIN
        WSACleanup();
#endif
        return 0;
    }

    QLocalServer commandServer;
    commandServer.setSocketOptions(QLocalServer::UserAccessOption);
    bool ownsCommandServer = commandServer.listen(commandServerName());
    if (!ownsCommandServer && !commandLineRequest.isEmpty()
        && forwardCommand(commandLineRequest)) {
        ssh_finalize();
#ifdef Q_OS_WIN
        WSACleanup();
#endif
        return 0;
    }
    if (!ownsCommandServer) {
        QLocalSocket probe;
        probe.connectToServer(commandServerName());
        if (!probe.waitForConnected(250)) {
            QLocalServer::removeServer(commandServerName());
            ownsCommandServer = commandServer.listen(commandServerName());
        }
    }

    FontManager::instance()->loadCachedFonts();
    app.setFont(clientoshUiFont(AppSettings::uiFontSize(), AppSettings::uiFontFamily()));

    Motion::loadFromSettings();

    MainWindow window;
    window.show();
    if (ownsCommandServer) {
        QObject::connect(&commandServer, &QLocalServer::newConnection, &window,
                         [&commandServer, &window]() {
            while (QLocalSocket* socket = commandServer.nextPendingConnection()) {
                const auto processRequests = [socket, &window]() {
                    while (socket->canReadLine()) {
                        const QJsonDocument doc = QJsonDocument::fromJson(socket->readLine().trimmed());
                        if (!doc.isObject()) {
                            continue;
                        }
                        const SessionProfile profile = profileFromCommand(doc.object());
                        if (profile.host.isEmpty()) {
                            continue;
                        }
                        if (window.isMinimized()) {
                            window.showNormal();
                        } else {
                            window.show();
                        }
                        window.raise();
                        window.activateWindow();
                        window.openSession(profile);
                    }
                };
                QObject::connect(socket, &QLocalSocket::readyRead, socket, processRequests);
                QObject::connect(socket, &QLocalSocket::disconnected,
                                 socket, &QObject::deleteLater);
                processRequests();
            }
        });
    }
    if (!commandLineRequest.isEmpty()) {
        const SessionProfile commandLineProfile = profileFromCommand(commandLineRequest);
        QTimer::singleShot(0, &window, [&window, commandLineProfile]() {
            if (!commandLineProfile.host.isEmpty()) {
                window.openSession(commandLineProfile);
            }
        });
    }

    const int code = app.exec();

    ssh_finalize();

#ifdef Q_OS_WIN
    WSACleanup();
#endif
    return code;
}
