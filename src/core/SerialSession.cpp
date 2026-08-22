#include "SerialSession.h"

#include <QDir>
#include <QMutexLocker>
#include <QSet>

#ifdef Q_OS_WIN
#  define NOMINMAX
#  include <windows.h>
#else
#  include <cerrno>
#  include <cstring>
#  include <fcntl.h>
#  include <poll.h>
#  include <termios.h>
#  include <unistd.h>
#endif

namespace {
#ifndef Q_OS_WIN
bool baudConstant(int baud, speed_t* value)
{
    switch (baud) {
    case 1200: *value = B1200; return true;
    case 2400: *value = B2400; return true;
    case 4800: *value = B4800; return true;
    case 9600: *value = B9600; return true;
    case 19200: *value = B19200; return true;
    case 38400: *value = B38400; return true;
    case 57600: *value = B57600; return true;
    case 115200: *value = B115200; return true;
#ifdef B230400
    case 230400: *value = B230400; return true;
#endif
#ifdef B460800
    case 460800: *value = B460800; return true;
#endif
#ifdef B921600
    case 921600: *value = B921600; return true;
#endif
    default: return false;
    }
}
#endif
}

SerialSession::SerialSession(QObject* parent) : QThread(parent) {}

SerialSession::~SerialSession()
{
    disconnectFromHost();
    wait(3000);
}

QStringList SerialSession::availablePorts()
{
    QStringList ports;
#ifdef Q_OS_WIN
    wchar_t target[512];
    for (int i = 1; i <= 256; ++i) {
        const QString name = QStringLiteral("COM%1").arg(i);
        if (QueryDosDeviceW(reinterpret_cast<LPCWSTR>(name.utf16()), target, 512) != 0) {
            ports.append(name);
        }
    }
#else
    QDir dev(QStringLiteral("/dev"));
    const QStringList filters = {
#ifdef Q_OS_MACOS
        QStringLiteral("cu.*"), QStringLiteral("tty.*")
#else
        QStringLiteral("ttyS*"), QStringLiteral("ttyUSB*"), QStringLiteral("ttyACM*"),
        QStringLiteral("ttyAMA*"), QStringLiteral("rfcomm*")
#endif
    };
    for (const QString& entry : dev.entryList(filters, QDir::System | QDir::Files | QDir::NoDotAndDotDot,
                                               QDir::Name)) {
        ports.append(dev.absoluteFilePath(entry));
    }
#endif
    ports.removeDuplicates();
    return ports;
}

void SerialSession::connectTo(const SessionProfile& profile)
{
    if (isRunning()) {
        disconnectFromHost();
        wait(3000);
    }
    {
        QMutexLocker lock(&m_mutex);
        m_profile = profile;
        m_outgoing.clear();
        m_stop = false;
        m_connected = false;
    }
    start();
}

void SerialSession::disconnectFromHost()
{
    QMutexLocker lock(&m_mutex);
    m_stop = true;
}

void SerialSession::sendData(const QByteArray& data)
{
    QMutexLocker lock(&m_mutex);
    m_outgoing.append(data);
}

bool SerialSession::isConnected() const
{
    QMutexLocker lock(&m_mutex);
    return m_connected;
}

void SerialSession::run()
{
    SessionProfile profile;
    {
        QMutexLocker lock(&m_mutex);
        profile = m_profile;
    }
    const QString device = profile.host.trimmed();
    emit statusChanged(QStringLiteral("opening %1…").arg(device));

#ifdef Q_OS_WIN
    QString nativeName = device;
    if (nativeName.startsWith(QLatin1String("COM"), Qt::CaseInsensitive)
        && nativeName.mid(3).toInt() >= 10) {
        nativeName.prepend(QStringLiteral("\\\\.\\"));
    }
    HANDLE handle = CreateFileW(reinterpret_cast<LPCWSTR>(nativeName.utf16()),
                                GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        emit errorOccurred(QStringLiteral("cannot open %1 (Windows error %2)")
                               .arg(device).arg(GetLastError()));
        return;
    }

    DCB dcb{};
    dcb.DCBlength = sizeof(DCB);
    if (!GetCommState(handle, &dcb)) {
        CloseHandle(handle);
        emit errorOccurred(QStringLiteral("cannot read serial settings for %1").arg(device));
        return;
    }
    dcb.BaudRate = static_cast<DWORD>(profile.serialBaudRate);
    dcb.ByteSize = static_cast<BYTE>(profile.serialDataBits);
    dcb.StopBits = profile.serialStopBits == 2 ? TWOSTOPBITS : ONESTOPBIT;
    const QString parity = profile.serialParity.toLower();
    dcb.Parity = parity == QLatin1String("even") ? EVENPARITY
        : parity == QLatin1String("odd") ? ODDPARITY
        : parity == QLatin1String("mark") ? MARKPARITY
        : parity == QLatin1String("space") ? SPACEPARITY : NOPARITY;
    dcb.fParity = dcb.Parity != NOPARITY;
    const QString flow = profile.serialFlowControl.toLower();
    dcb.fOutxCtsFlow = flow == QLatin1String("hardware");
    dcb.fRtsControl = dcb.fOutxCtsFlow ? RTS_CONTROL_HANDSHAKE : RTS_CONTROL_ENABLE;
    dcb.fOutX = dcb.fInX = flow == QLatin1String("software");
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    if (!SetCommState(handle, &dcb)) {
        CloseHandle(handle);
        emit errorOccurred(QStringLiteral("unsupported serial settings for %1").arg(device));
        return;
    }
    COMMTIMEOUTS timeouts{};
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutConstant = 25;
    timeouts.WriteTotalTimeoutConstant = 1000;
    SetCommTimeouts(handle, &timeouts);
    PurgeComm(handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
#else
    const QByteArray path = device.toLocal8Bit();
    const int handle = ::open(path.constData(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (handle < 0) {
        emit errorOccurred(QStringLiteral("cannot open %1: %2")
                               .arg(device, QString::fromLocal8Bit(std::strerror(errno))));
        return;
    }
    termios tio{};
    if (tcgetattr(handle, &tio) != 0) {
        const QString error = QString::fromLocal8Bit(std::strerror(errno));
        ::close(handle);
        emit errorOccurred(QStringLiteral("cannot read serial settings: %1").arg(error));
        return;
    }
    cfmakeraw(&tio);
    speed_t speed{};
    if (!baudConstant(profile.serialBaudRate, &speed)) {
        ::close(handle);
        emit errorOccurred(QStringLiteral("unsupported baud rate: %1").arg(profile.serialBaudRate));
        return;
    }
    cfsetispeed(&tio, speed);
    cfsetospeed(&tio, speed);
    tio.c_cflag &= ~CSIZE;
    tio.c_cflag |= profile.serialDataBits == 5 ? CS5 : profile.serialDataBits == 6 ? CS6
        : profile.serialDataBits == 7 ? CS7 : CS8;
    tio.c_cflag |= CLOCAL | CREAD;
    tio.c_cflag &= ~(PARENB | PARODD | CSTOPB);
    const QString parity = profile.serialParity.toLower();
    if (parity == QLatin1String("even")) tio.c_cflag |= PARENB;
    if (parity == QLatin1String("odd")) tio.c_cflag |= PARENB | PARODD;
    if (profile.serialStopBits == 2) tio.c_cflag |= CSTOPB;
#ifdef CRTSCTS
    tio.c_cflag &= ~CRTSCTS;
    if (profile.serialFlowControl == QLatin1String("hardware")) tio.c_cflag |= CRTSCTS;
#endif
    tio.c_iflag &= ~(IXON | IXOFF | IXANY);
    if (profile.serialFlowControl == QLatin1String("software")) tio.c_iflag |= IXON | IXOFF;
    if (tcsetattr(handle, TCSANOW, &tio) != 0) {
        const QString error = QString::fromLocal8Bit(std::strerror(errno));
        ::close(handle);
        emit errorOccurred(QStringLiteral("cannot apply serial settings: %1").arg(error));
        return;
    }
    tcflush(handle, TCIOFLUSH);
#endif

    {
        QMutexLocker lock(&m_mutex);
        m_connected = true;
    }
    emit connected();
    emit statusChanged(QStringLiteral("connected · Serial %1 @ %2").arg(device).arg(profile.serialBaudRate));

    while (true) {
        QByteArray outgoing;
        {
            QMutexLocker lock(&m_mutex);
            if (m_stop) break;
            outgoing.swap(m_outgoing);
        }
        if (!outgoing.isEmpty()) {
#ifdef Q_OS_WIN
            DWORD written = 0;
            if (!WriteFile(handle, outgoing.constData(), static_cast<DWORD>(outgoing.size()), &written, nullptr)) {
                emit errorOccurred(QStringLiteral("serial write failed (Windows error %1)").arg(GetLastError()));
                break;
            }
#else
            qsizetype offset = 0;
            while (offset < outgoing.size()) {
                const ssize_t n = ::write(handle, outgoing.constData() + offset,
                                          static_cast<size_t>(outgoing.size() - offset));
                if (n > 0) offset += n;
                else if (errno != EAGAIN && errno != EINTR) {
                    emit errorOccurred(QStringLiteral("serial write failed: %1")
                                           .arg(QString::fromLocal8Bit(std::strerror(errno))));
                    break;
                }
            }
#endif
        }

        char buffer[4096];
#ifdef Q_OS_WIN
        DWORD received = 0;
        if (!ReadFile(handle, buffer, sizeof(buffer), &received, nullptr)) {
            emit errorOccurred(QStringLiteral("serial read failed (Windows error %1)").arg(GetLastError()));
            break;
        }
        if (received > 0) emit dataReceived(QByteArray(buffer, static_cast<qsizetype>(received)));
#else
        pollfd descriptor{handle, POLLIN, 0};
        const int ready = ::poll(&descriptor, 1, 25);
        if (ready > 0 && (descriptor.revents & POLLIN)) {
            const ssize_t received = ::read(handle, buffer, sizeof(buffer));
            if (received > 0) emit dataReceived(QByteArray(buffer, received));
        } else if (ready < 0 && errno != EINTR) {
            emit errorOccurred(QStringLiteral("serial read failed: %1")
                                   .arg(QString::fromLocal8Bit(std::strerror(errno))));
            break;
        }
#endif
    }

#ifdef Q_OS_WIN
    CloseHandle(handle);
#else
    ::close(handle);
#endif
    {
        QMutexLocker lock(&m_mutex);
        m_connected = false;
    }
    emit disconnected();
    emit statusChanged(QStringLiteral("disconnected"));
}
