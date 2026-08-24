#include "KeyringAdapter.h"

#include <QCoreApplication>
#include <QTemporaryDir>

#include <cassert>

int main(int argc, char** argv)
{
    QTemporaryDir dataDir;
    assert(dataDir.isValid());
    qputenv("XDG_DATA_HOME", dataDir.path().toUtf8());

    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("clientosh-test"));
    QCoreApplication::setApplicationName(QStringLiteral("vault-test"));

    const QString entry = QStringLiteral("clientosh-test/fallback-entry");
    const QByteArray secret = QByteArrayLiteral("test-secret-value");
    assert(KeyringAdapter::storeFallback(entry, secret));

    QByteArray loaded;
    assert(KeyringAdapter::retrieveFallback(entry, loaded));
    assert(loaded == secret);
    loaded.fill('\0');
    assert(KeyringAdapter::removeFallback(entry));
    assert(!KeyringAdapter::retrieveFallback(entry, loaded));

    return 0;
}
