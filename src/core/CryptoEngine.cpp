#include "CryptoEngine.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QRandomGenerator>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <openssl/evp.h>

#if defined(Q_OS_WIN)
#include <windows.h>
#elif defined(Q_OS_MACOS)
#include <IOKit/IOKitLib.h>
#endif

namespace {

void clearBuffer(QByteArray& data)
{
    if (!data.isNull()) {
        std::memset(data.data(), 0, size_t(data.size()));
        data.clear();
    }
}

inline bool keyValid(const QByteArray& key)
{
    return key.size() == CryptoEngine::kKeyBytes;
}

} // namespace

bool CryptoEngine::blobHasSalt(const QByteArray& blob)
{
    return blob.size() > (kSaltBytes + kNonceBytes + kTagBytes);
}

QByteArray CryptoEngine::encrypt(const QByteArray& key, const QByteArray& plaintext)
{
    if (!keyValid(key)) {
        throw CryptoError{QStringLiteral("invalid key length %1").arg(key.size())};
    }

    // salt is stored with the blob for callers that derive a per-file key.
    QByteArray salt(kSaltBytes, Qt::Uninitialized);
    for (int i = 0; i < kSaltBytes; ++i) {
        salt[i] = char(QRandomGenerator::global()->bounded(256));
    }
    QByteArray nonce(kNonceBytes, Qt::Uninitialized);
    for (int i = 0; i < kNonceBytes; ++i) {
        nonce[i] = char(QRandomGenerator::global()->bounded(256));
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw CryptoError{QStringLiteral("EVP_CIPHER_CTX_new failed")};
    }

    QByteArray out;
    out.resize(kSaltBytes + kNonceBytes + kTagBytes + plaintext.size());
    int encl = 0;
    bool ok = false;

    unsigned char* outPtr = reinterpret_cast<unsigned char*>(out.data());

    auto cleanup = [&]() {
        EVP_CIPHER_CTX_free(ctx);
        if (!ok) {
            clearBuffer(out);
        }
    };

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1)
        { cleanup(); throw CryptoError{QStringLiteral("AES-GCM init failed")}; }
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kNonceBytes, nullptr) != 1)
        { cleanup(); throw CryptoError{QStringLiteral("AES-GCM set ivlen failed")}; }

    const auto* kp = reinterpret_cast<const unsigned char*>(key.constData());
    const auto* np = reinterpret_cast<const unsigned char*>(nonce.constData());
    if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, kp, np) != 1)
        { cleanup(); throw CryptoError{QStringLiteral("AES-GCM key/iv failed")}; }

    int tmpLen = 0;
    const auto* pt = reinterpret_cast<const unsigned char*>(plaintext.constData());
    if (EVP_EncryptUpdate(ctx, outPtr + kSaltBytes + kNonceBytes + kTagBytes, &tmpLen, pt, int(plaintext.size())) != 1)
        { cleanup(); throw CryptoError{QStringLiteral("AES-GCM update failed")}; }
    encl = tmpLen;
    int fLen = 0;
    if (EVP_EncryptFinal_ex(ctx, outPtr + kSaltBytes + kNonceBytes + kTagBytes + encl, &fLen) != 1)
        { cleanup(); throw CryptoError{QStringLiteral("AES-GCM final failed")}; }
    encl += fLen;

    unsigned char tag[kTagBytes];
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, kTagBytes, tag) != 1)
        { cleanup(); throw CryptoError{QStringLiteral("AES-GCM get tag failed")}; }

    std::memcpy(outPtr, salt.constData(), kSaltBytes);
    std::memcpy(outPtr + kSaltBytes, nonce.constData(), kNonceBytes);
    std::memcpy(outPtr + kSaltBytes + kNonceBytes, tag, kTagBytes);
    out.truncate(kSaltBytes + kNonceBytes + kTagBytes + encl);

    ok = true;
    cleanup();
    return out;
}

QByteArray CryptoEngine::decrypt(const QByteArray& key, const QByteArray& blob)
{
    if (!keyValid(key)) {
        throw CryptoError{QStringLiteral("invalid key length %1").arg(key.size())};
    }
    if (blob.size() < kSaltBytes + kNonceBytes + kTagBytes) {
        throw CryptoError{QStringLiteral("ciphertext too short")};
    }

    const int nonceOff = kSaltBytes;
    const int tagOff = kSaltBytes + kNonceBytes;
    const int ctxtOff = kSaltBytes + kNonceBytes + kTagBytes;
    const int ctxtLen = blob.size() - ctxtOff;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw CryptoError{QStringLiteral("EVP_CIPHER_CTX_new failed")};
    }

    QByteArray out;
    out.resize(ctxtLen + kTagBytes);
    int declen = 0;
    bool ok = false;

    unsigned char* outPtr = reinterpret_cast<unsigned char*>(out.data());
    const auto* kp = reinterpret_cast<const unsigned char*>(key.constData());
    const auto* np = reinterpret_cast<const unsigned char*>(blob.constData() + nonceOff);
    const auto* tagPtr = reinterpret_cast<const unsigned char*>(blob.constData() + tagOff);
    const auto* ct = reinterpret_cast<const unsigned char*>(blob.constData() + ctxtOff);

    auto cleanup = [&]() {
        EVP_CIPHER_CTX_free(ctx);
        if (!ok) {
            clearBuffer(out);
        }
    };

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1)
        { cleanup(); throw CryptoError{QStringLiteral("AES-GCM init failed")}; }
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kNonceBytes, nullptr) != 1)
        { cleanup(); throw CryptoError{QStringLiteral("AES-GCM set ivlen failed")}; }
    if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, kp, np) != 1)
        { cleanup(); throw CryptoError{QStringLiteral("AES-GCM key/iv failed")}; }

    int tmpLen = 0;
    if (ctxtLen > 0
        && EVP_DecryptUpdate(ctx, outPtr, &tmpLen, ct, ctxtLen) != 1) {
        cleanup();
        throw CryptoError{QStringLiteral("AES-GCM update failed")};
    }
    declen = tmpLen;
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, kTagBytes, const_cast<unsigned char*>(tagPtr)) != 1)
        { cleanup(); throw CryptoError{QStringLiteral("AES-GCM set tag failed")}; }

    if (EVP_DecryptFinal_ex(ctx, outPtr + declen, &tmpLen) > 0) {
        declen += tmpLen;
        out.truncate(declen);
        ok = true;
    } else {
        cleanup();
        throw CryptoError{QStringLiteral("AES-256-GCM decrypt failed (wrong key or corrupted data)")};
    }

    ok = true;
    cleanup();
    return out;
}

QByteArray CryptoEngine::sha256(const QByteArray& data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256);
}

QString CryptoEngine::machineIdentifier()
{
#if defined(Q_OS_WIN)
    HKEY key = nullptr;
    QString guid = QStringLiteral("win-machine");
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography",
                      0, KEY_READ | KEY_WOW64_64KEY, &key) == ERROR_SUCCESS) {
        DWORD size = 0;
        if (RegQueryValueExW(key, L"MachineGuid", nullptr, nullptr, nullptr, &size) == ERROR_SUCCESS
            && size > 0) {
            QByteArray buf(int(size), Qt::Uninitialized);
            WCHAR* wbuf = reinterpret_cast<WCHAR*>(buf.data());
            DWORD wlen = size;
            if (RegQueryValueExW(key, L"MachineGuid", nullptr, nullptr,
                                 reinterpret_cast<LPBYTE>(wbuf), &wlen) == ERROR_SUCCESS) {
                int chars = int(wlen) / int(sizeof(WCHAR));
                // wlen includes the NUL terminator; drop it so the GUID is exact.
                if (chars > 0 && wbuf[chars - 1] == L'\0') {
                    --chars;
                }
                guid = QString::fromWCharArray(wbuf, chars).trimmed();
            }
        }
        RegCloseKey(key);
    }
    return guid;
#elif defined(Q_OS_MACOS)
    QString uuid = QStringLiteral("mac-machine");
#if defined(MAC_OS_VERSION_12_0) && MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_VERSION_12_0
    const mach_port_t ioPort = kIOMainPortDefault;
#else
    const mach_port_t ioPort = kIOMasterPortDefault;
#endif
    io_registry_entry_t entry = IORegistryEntryFromPath(ioPort, "IOService:/");
    if (entry) {
        CFStringRef uuidRef = static_cast<CFStringRef>(
            IORegistryEntryCreateCFProperty(entry, CFSTR("IOPlatformUUID"), kCFAllocatorDefault, 0));
        if (uuidRef) {
            uuid = QString::fromCFString(uuidRef);
            CFRelease(uuidRef);
        }
        IOObjectRelease(entry);
    }
    return uuid;
#else
    QString id;
    for (const char* path : {"/etc/machine-id", "/var/lib/dbus/machine-id"}) {
        QFile f(QString::fromLatin1(path));
        if (f.open(QIODevice::ReadOnly)) {
            const QByteArray raw = f.readAll().trimmed();
            if (!raw.isEmpty()) {
                id = QString::fromLatin1(raw);
                break;
            }
        }
    }
    if (id.isEmpty()) {
        id = QStringLiteral("linux-machine");
    }
    return id;
#endif
}

QString currentUserScope()
{
    // Basename of the home dir is a stable, portable per-user identifier that
    // works on all three platforms without shelling out for a SID/lookup.
    const QString home = QDir::homePath();
    const QString base = home.section(QLatin1Char('/'), -1);
    if (base.isEmpty()) {
        return home;
    }
    return base;
}

QByteArray CryptoEngine::deriveMachineBoundKey(const QStringList& extraScope)
{
    const QString purpose = QStringLiteral("clientosh::connects::v1");
    QByteArray seed = purpose.toUtf8();
    seed.append('|');
    seed.append(machineIdentifier().toUtf8());
    seed.append('|');
    seed.append(currentUserScope().toUtf8());

    for (const QString& s : extraScope) {
        seed.append('|');
        seed.append(s.toUtf8());
    }

    QByteArray key = sha256(seed);
    key.resize(kKeyBytes);
    return key;
}
