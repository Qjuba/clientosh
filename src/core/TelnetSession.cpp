#include "TelnetSession.h"

#include <QTcpSocket>

namespace {
constexpr char IAC = static_cast<char>(255);
constexpr char WILL = static_cast<char>(251);
constexpr char WONT = static_cast<char>(252);
constexpr char DO = static_cast<char>(253);
constexpr char DONT = static_cast<char>(254);
constexpr char SB = static_cast<char>(250);
constexpr char SE = static_cast<char>(240);
constexpr char NAWS = static_cast<char>(31);
constexpr char ECHO = static_cast<char>(1);
constexpr char SGA = static_cast<char>(3);

QByteArray telnetCommand(char cmd, char opt)
{
    QByteArray out;
    out.append(IAC);
    out.append(cmd);
    out.append(opt);
    return out;
}

QByteArray telnetNaws(int cols, int rows)
{
    const int c = qBound(1, cols, 65535);
    const int r = qBound(1, rows, 65535);
    QByteArray out;
    out.reserve(9);
    out.append(IAC);
    out.append(SB);
    out.append(NAWS);
    out.append(static_cast<char>((c >> 8) & 0xff));
    out.append(static_cast<char>(c & 0xff));
    out.append(static_cast<char>((r >> 8) & 0xff));
    out.append(static_cast<char>(r & 0xff));
    out.append(IAC);
    out.append(SE);
    return out;
}

QByteArray escapeOutgoing(const QByteArray& data)
{
    if (!data.contains(IAC)) {
        return data;
    }
    QByteArray out;
    out.reserve(data.size() + 8);
    for (char ch : data) {
        out.append(ch);
        if (static_cast<unsigned char>(ch) == 255) {
            out.append(IAC);
        }
    }
    return out;
}

/** Strip telnet IAC sequences; emit user-visible bytes via outUser. Returns commands to send. */
struct TelnetParser {
    enum class State { Normal, Iac, Will, Wont, Do, Dont, Sb, SbIac };
    State state = State::Normal;
    char option = 0;

    QByteArray ingest(const QByteArray& chunk, int cols, int rows)
    {
        QByteArray replies;
        QByteArray* outUser = nullptr;
        Q_UNUSED(outUser);
        for (unsigned char byte : chunk) {
            switch (state) {
            case State::Normal:
                if (byte == 255) {
                    state = State::Iac;
                } else if (m_userOut) {
                    m_userOut->append(static_cast<char>(byte));
                }
                break;
            case State::Iac:
                if (byte == 255) {
                    if (m_userOut) {
                        m_userOut->append(IAC);
                    }
                    state = State::Normal;
                } else if (byte == 251) {
                    state = State::Will;
                } else if (byte == 252) {
                    state = State::Wont;
                } else if (byte == 253) {
                    state = State::Do;
                } else if (byte == 254) {
                    state = State::Dont;
                } else if (byte == 250) {
                    state = State::Sb;
                } else {
                    state = State::Normal;
                }
                break;
            case State::Will:
                option = static_cast<char>(byte);
                if (option == ECHO) {
                    replies.append(telnetCommand(DO, ECHO));
                } else if (option == SGA) {
                    replies.append(telnetCommand(DO, SGA));
                } else {
                    replies.append(telnetCommand(DONT, option));
                }
                state = State::Normal;
                break;
            case State::Wont:
                option = static_cast<char>(byte);
                replies.append(telnetCommand(DONT, option));
                state = State::Normal;
                break;
            case State::Do:
                option = static_cast<char>(byte);
                if (option == NAWS) {
                    replies.append(telnetCommand(WILL, NAWS));
                    replies.append(telnetNaws(cols, rows));
                } else if (option == SGA) {
                    replies.append(telnetCommand(WILL, SGA));
                } else if (option == ECHO) {
                    replies.append(telnetCommand(WILL, ECHO));
                } else {
                    replies.append(telnetCommand(WONT, option));
                }
                state = State::Normal;
                break;
            case State::Dont:
                option = static_cast<char>(byte);
                replies.append(telnetCommand(WONT, option));
                state = State::Normal;
                break;
            case State::Sb:
                if (byte == 255) {
                    state = State::SbIac;
                }
                break;
            case State::SbIac:
                state = byte == 240 ? State::Normal : State::Sb;
                break;
            }
        }
        return replies;
    }

    QByteArray* m_userOut = nullptr;
};

QByteArray processTelnetIncoming(TelnetParser* parser, const QByteArray& chunk, QByteArray* outUser,
                                 int cols, int rows)
{
    parser->m_userOut = outUser;
    outUser->clear();
    return parser->ingest(chunk, cols, rows);
}
} // namespace

TelnetSession::TelnetSession(QObject* parent)
    : QThread(parent)
{
}

TelnetSession::~TelnetSession()
{
    disconnectFromHost();
    if (!wait(8000)) {
        terminate();
        wait(2000);
    }
}

bool TelnetSession::isConnected() const
{
    QMutexLocker lock(&m_mutex);
    return m_connected;
}

bool TelnetSession::stopRequested() const
{
    QMutexLocker lock(&m_mutex);
    return m_stopRequested;
}

void TelnetSession::abortTransport(QTcpSocket* socket)
{
    if (!socket) {
        return;
    }
    socket->abort();
}

void TelnetSession::connectTo(const QString& host, int port, const QString& user,
                              const QString& password)
{
    bool needStart = false;
    {
        QMutexLocker lock(&m_mutex);
        m_host = host;
        m_port = port;
        m_user = user;
        m_password = password;
        m_pendingWrite.clear();
        m_resizePending = false;

        if (isRunning()) {
            m_restartRequested = true;
            m_stopRequested = true;
        } else {
            m_stopRequested = false;
            m_restartRequested = false;
            needStart = true;
        }
    }

    if (needStart) {
        start();
    }
}

void TelnetSession::disconnectFromHost()
{
    QMutexLocker lock(&m_mutex);
    m_stopRequested = true;
    m_restartRequested = false;
}

void TelnetSession::sendData(const QByteArray& data)
{
    QMutexLocker lock(&m_mutex);
    m_pendingWrite.append(data);
}

void TelnetSession::resizePty(int cols, int rows)
{
    QMutexLocker lock(&m_mutex);
    m_cols = qMax(1, cols);
    m_rows = qMax(1, rows);
    m_resizePending = true;
}

void TelnetSession::run()
{
    for (;;) {
        QString host;
        QString user;
        QString password;
        int port = 23;
        int cols = 80;
        int rows = 24;

        {
            QMutexLocker lock(&m_mutex);
            host = m_host;
            user = m_user;
            password = m_password;
            port = m_port;
            cols = m_cols;
            rows = m_rows;
            m_stopRequested = false;
            m_restartRequested = false;
            m_connected = false;
        }

        emit statusChanged(QStringLiteral("connecting to %1:%2...").arg(host).arg(port));

        QTcpSocket socket;
        socket.connectToHost(host, port);
        if (!socket.waitForConnected(12000)) {
            if (!stopRequested()) {
                emit errorOccurred(QStringLiteral("connect failed: %1").arg(socket.errorString()));
            }
        } else if (stopRequested()) {
            socket.disconnectFromHost();
        } else {
            {
                QMutexLocker lock(&m_mutex);
                m_connected = true;
            }

            emit connected();
            emit statusChanged(QStringLiteral("connected · Telnet %1:%2").arg(host).arg(port));

            TelnetParser parser;

            // Optional inline login (device-dependent; user can always type manually).
            if (!user.trimmed().isEmpty()) {
                socket.write(escapeOutgoing((user.trimmed() + QStringLiteral("\r\n")).toUtf8()));
                socket.flush();
            }
            if (!password.isEmpty()) {
                socket.waitForReadyRead(400);
                socket.write(escapeOutgoing((password + QStringLiteral("\r\n")).toUtf8()));
                socket.flush();
            }

            while (true) {
                bool stop = false;
                QByteArray toWrite;
                bool doResize = false;
                int newCols = cols;
                int newRows = rows;

                {
                    QMutexLocker lock(&m_mutex);
                    stop = m_stopRequested;
                    if (!m_pendingWrite.isEmpty()) {
                        toWrite.swap(m_pendingWrite);
                    }
                    if (m_resizePending) {
                        doResize = true;
                        newCols = m_cols;
                        newRows = m_rows;
                        m_resizePending = false;
                    }
                }

                if (stop) {
                    break;
                }

                if (doResize) {
                    cols = newCols;
                    rows = newRows;
                    socket.write(telnetNaws(cols, rows));
                }

                if (!toWrite.isEmpty()) {
                    socket.write(escapeOutgoing(toWrite));
                    socket.flush();
                }

                if (socket.bytesAvailable() > 0 || socket.waitForReadyRead(50)) {
                    const QByteArray chunk = socket.readAll();
                    if (!chunk.isEmpty()) {
                        QByteArray userData;
                        const QByteArray replies =
                            processTelnetIncoming(&parser, chunk, &userData, cols, rows);
                        if (!replies.isEmpty()) {
                            socket.write(replies);
                            socket.flush();
                        }
                        if (!userData.isEmpty()) {
                            emit dataReceived(userData);
                        }
                    }
                }

                if (socket.state() != QAbstractSocket::ConnectedState) {
                    break;
                }
            }

            {
                QMutexLocker lock(&m_mutex);
                m_connected = false;
            }

            socket.disconnectFromHost();
            emit disconnected();
            emit statusChanged(QStringLiteral("disconnected"));
        }

        bool restart = false;
        {
            QMutexLocker lock(&m_mutex);
            restart = m_restartRequested;
            if (restart) {
                m_restartRequested = false;
                m_stopRequested = false;
            }
        }
        if (!restart) {
            break;
        }
        emit statusChanged(QStringLiteral("reconnecting..."));
    }
}
