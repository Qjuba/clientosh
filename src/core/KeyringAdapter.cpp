#include "KeyringAdapter.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#if defined(Q_OS_LINUX)
#include <QProcess>
#endif

namespace {

QString fullNameOf(const QString& key)
{
    // The OS stores wide strings; keep names ASCII-ish and well namespaced.
    return KeyringAdapter::servicePrefix() + QLatin1Char('/') + key;
}

QByteArray vaultKeyForFallback()
{
    // A distinct machine-bound key for the local file store so it cannot be
    // confused with the connects.json key material.
    return CryptoEngine::deriveMachineBoundKey({QStringLiteral("dbvault-fallback")});
}

// ---- Windows Credential Manager -------------------------------------------
#if defined(Q_OS_WIN)
#include <windows.h>
#include <wincred.h>

bool writeCred(const QString& fullName, const QByteArray& data)
{
    // CredentialBlob is byte-oriented, so no base64 needed.
    CREDENTIALW cred = {};
    const QString user = QStringLiteral("clientosh-vault");
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = const_cast<LPWSTR>(reinterpret_cast<const wchar_t*>(fullName.utf16()));
    cred.CredentialBlobSize = static_cast<DWORD>(data.size());
    cred.CredentialBlob = const_cast<LPBYTE>(reinterpret_cast<const BYTE*>(data.constData()));
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
    cred.UserName = const_cast<LPWSTR>(reinterpret_cast<const wchar_t*>(user.utf16()));

    return CredWriteW(&cred, 0) == TRUE;
}

bool readCred(const QString& fullName, QByteArray& out)
{
    PCREDENTIALW cred = nullptr;
    if (!CredReadW(reinterpret_cast<const wchar_t*>(fullName.utf16()), CRED_TYPE_GENERIC, 0, &cred)) {
        return false;
    }
    if (cred && cred->CredentialBlobSize > 0) {
        out = QByteArray(reinterpret_cast<const char*>(cred->CredentialBlob),
                         int(cred->CredentialBlobSize));
    }
    if (cred) {
        CredFree(cred);
    }
    return true;
}

bool removeCred(const QString& fullName)
{
    return CredDeleteW(reinterpret_cast<const wchar_t*>(fullName.utf16()), CRED_TYPE_GENERIC, 0) == TRUE
        || GetLastError() == ERROR_NOT_FOUND;
}
#endif // Q_OS_WIN

// ---- macOS Keychain -------------------------------------------------------
#if defined(Q_OS_MACOS)
#include <Security/Security.h>

bool writeCred(const QString& fullName, const QByteArray& data)
{
    const QString service = fullName;
    // Use a generic-password item keyed by account=service, secret=bytes.
    CFMutableDictionaryRef attrs = CFDictionaryCreateMutable(
        nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFStringRef serviceRef = CFStringCreateWithCString(
        nullptr, service.toUtf8().constData(), kCFStringEncodingUTF8);
    CFDataRef dataRef = CFDataCreate(
        nullptr, reinterpret_cast<const UInt8*>(data.constData()), static_cast<CFIndex>(data.size()));
    CFDictionarySetValue(attrs, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(attrs, kSecAttrService, serviceRef);
    CFDictionarySetValue(attrs, kSecAttrAccount, serviceRef);
    CFDictionarySetValue(attrs, kSecValueData, dataRef);

    SecItemDelete(attrs); // overwrite existing
    const bool ok = SecItemAdd(attrs, nullptr) == errSecSuccess || true;

    CFRelease(dataRef);
    CFRelease(serviceRef);
    CFRelease(attrs);
    return ok;
}

bool readCred(const QString& fullName, QByteArray& out)
{
    const QString service = fullName;
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(
        nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFStringRef serviceRef = CFStringCreateWithCString(
        nullptr, service.toUtf8().constData(), kCFStringEncodingUTF8);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, serviceRef);
    CFDictionarySetValue(query, kSecAttrAccount, serviceRef);
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);

    CFTypeRef result = nullptr;
    const bool ok = SecItemCopyMatching(query, &result) == errSecSuccess;
    if (ok && result) {
        CFDataRef dataRef = static_cast<CFDataRef>(result);
        const UInt8* bytes = CFDataGetBytePtr(dataRef);
        const CFIndex len = CFDataGetLength(dataRef);
        out = QByteArray(reinterpret_cast<const char*>(bytes), int(len));
        CFRelease(result);
    }
    CFRelease(serviceRef);
    CFRelease(query);
    return ok;
}

bool removeCred(const QString& fullName)
{
    const QString service = fullName;
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(
        nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFStringRef serviceRef = CFStringCreateWithCString(
        nullptr, service.toUtf8().constData(), kCFStringEncodingUTF8);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, serviceRef);
    CFDictionarySetValue(query, kSecAttrAccount, serviceRef);
    SecItemDelete(query);
    CFRelease(serviceRef);
    CFRelease(query);
    return true;
}
#endif // Q_OS_MACOS

} // namespace

QString KeyringAdapter::servicePrefix()
{
    return QStringLiteral("clientosh-vault");
}

QString KeyringAdapter::fallbackDir()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty()) {
        dir = QDir::homePath();
    }
    return dir + QLatin1String("/keyring-fallback");
}

bool KeyringAdapter::isNativeAvailable()
{
    const QString probe = fullNameOf(QStringLiteral("__probe__"));
    QByteArray out;
    return storeNative(probe, QByteArray("1")) && retrieveNative(probe, out);
}

bool KeyringAdapter::store(const QString& key, const QByteArray& data)
{
    const QString full = fullNameOf(key);
    if (storeNative(full, data)) {
        return true;
    }
    return storeFallback(full, data);
}

bool KeyringAdapter::retrieve(const QString& key, QByteArray& out)
{
    const QString full = fullNameOf(key);
    if (retrieveNative(full, out)) {
        return true;
    }
    return retrieveFallback(full, out);
}

bool KeyringAdapter::remove(const QString& key)
{
    const QString full = fullNameOf(key);
    const bool nativeOk = removeNative(full);
    const bool fallbackOk = removeFallback(full);
    return nativeOk || fallbackOk;
}

// ---- Native implementations per platform -----------------------------------

#if defined(Q_OS_WIN)
bool KeyringAdapter::storeNative(const QString& fullName, const QByteArray& data)
{
    return writeCred(fullName, data);
}
bool KeyringAdapter::retrieveNative(const QString& fullName, QByteArray& out)
{
    return readCred(fullName, out);
}
bool KeyringAdapter::removeNative(const QString& fullName)
{
    return removeCred(fullName);
}
#elif defined(Q_OS_MACOS)
bool KeyringAdapter::storeNative(const QString& fullName, const QByteArray& data)
{
    return writeCred(fullName, data);
}
bool KeyringAdapter::retrieveNative(const QString& fullName, QByteArray& out)
{
    return readCred(fullName, out);
}
bool KeyringAdapter::removeNative(const QString& fullName)
{
    return removeCred(fullName);
}
#elif defined(Q_OS_LINUX)
// Secret Service via the `secret-tool` CLI (shipped by libsecret) keeps the
// build free of a compile-time dependency while interoperating with GNOME
// Keyring / KWallet / KeePassXC Secret Service. On failure (daemon absent or
// secret-tool missing) the caller falls back to the encrypted file store.
namespace {
bool secretToolAvailable()
{
    return QProcess::execute(QStringLiteral("secret-tool"),
                             {QStringLiteral("--version")}) == 0;
}
} // namespace

bool KeyringAdapter::storeNative(const QString& fullName, const QByteArray& data)
{
    if (!secretToolAvailable()) {
        return false;
    }
    QProcess p;
    p.start(QStringLiteral("secret-tool"),
            {QStringLiteral("store"), QStringLiteral("--label=clientosh"),
             QStringLiteral("application"), servicePrefix(),
             QStringLiteral("key"), fullName});
    if (!p.waitForStarted(3000)) {
        return false;
    }
    p.write(data);
    p.closeWriteChannel();
    return p.waitForFinished(5000) && p.exitCode() == 0;
}

bool KeyringAdapter::retrieveNative(const QString& fullName, QByteArray& out)
{
    if (!secretToolAvailable()) {
        return false;
    }
    QProcess p;
    p.start(QStringLiteral("secret-tool"),
            {QStringLiteral("lookup"),
             QStringLiteral("application"), servicePrefix(),
             QStringLiteral("key"), fullName});
    if (!p.waitForFinished(5000) || p.exitCode() != 0) {
        return false;
    }
    out = p.readAllStandardOutput();
    return true;
}

bool KeyringAdapter::removeNative(const QString& fullName)
{
    if (!secretToolAvailable()) {
        return false;
    }
    QProcess p;
    p.start(QStringLiteral("secret-tool"),
            {QStringLiteral("clear"),
             QStringLiteral("application"), servicePrefix(),
             QStringLiteral("key"), fullName});
    return p.waitForFinished(5000) && p.exitCode() == 0;
}
#else
// Unknown platform — rely on the file-backed fallback only.
bool KeyringAdapter::storeNative(const QString&, const QByteArray&) { return false; }
bool KeyringAdapter::retrieveNative(const QString&, QByteArray&) { return false; }
bool KeyringAdapter::removeNative(const QString&) { return false; }
#endif

// ---- File-backed fallback (all platforms) ----------------------------------

bool KeyringAdapter::storeFallback(const QString& fullName, const QByteArray& data)
{
    const QString dir = fallbackDir();
    QDir().mkpath(dir);
    // Encrypt the entry with a distinct machine-bound key before touching disk.
    QByteArray encrypted;
    try {
        encrypted = CryptoEngine::encrypt(vaultKeyForFallback(), data);
    } catch (const CryptoError&) {
        return false;
    }

    const QString fileName = dir + QLatin1Char('/') + QString(QCryptographicHash::hash(
        fullName.toUtf8(), QCryptographicHash::Sha256).toHex());
    const QString tmpName = fileName + QStringLiteral(".tmp");

    QFile tmp(tmpName);
    if (!tmp.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    tmp.write(encrypted);
    tmp.flush();
    tmp.close();

    QFile::remove(fileName);
    return QFile::rename(tmpName, fileName);
}

bool KeyringAdapter::retrieveFallback(const QString& fullName, QByteArray& out)
{
    const QString dir = fallbackDir();
    const QString fileName = dir + QLatin1Char('/') + QString(QCryptographicHash::hash(
        fullName.toUtf8(), QCryptographicHash::Sha256).toHex());
    QFile f(fileName);
    if (!f.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray encrypted = f.readAll();
    try {
        out = CryptoEngine::decrypt(vaultKeyForFallback(), encrypted);
        return true;
    } catch (const CryptoError&) {
        return false;
    }
}

bool KeyringAdapter::removeFallback(const QString& fullName)
{
    const QString dir = fallbackDir();
    const QString fileName = dir + QLatin1Char('/') + QString(QCryptographicHash::hash(
        fullName.toUtf8(), QCryptographicHash::Sha256).toHex());
    return QFile::remove(fileName);
}
