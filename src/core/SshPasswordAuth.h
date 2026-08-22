#pragma once

#include <libssh/libssh.h>

#include <QString>

/**
 * Keyboard-interactive auth: answer secret prompts with password, visible prompts with username.
 * Returns true when libssh reports SSH_AUTH_SUCCESS.
 */
inline bool sshTryKbdintPassword(ssh_session session,
                                 const QString& password,
                                 const QString& username = {},
                                 QString* errorOut = nullptr)
{
    if (!session || password.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("keyboard-interactive auth requires a password");
        }
        return false;
    }

    const QByteArray passUtf8 = password.toUtf8();
    const QByteArray userUtf8 = username.toUtf8();

    for (;;) {
        const int rc = ssh_userauth_kbdint(session, nullptr, nullptr);
        if (rc == SSH_AUTH_SUCCESS) {
            return true;
        }
        if (rc != SSH_AUTH_INFO) {
            if (errorOut) {
                *errorOut = QString::fromUtf8(ssh_get_error(session));
            }
            return false;
        }

        const unsigned int nprompts = static_cast<unsigned int>(ssh_userauth_kbdint_getnprompts(session));
        if (nprompts == 0) {
            if (errorOut) {
                *errorOut = QStringLiteral("keyboard-interactive auth returned no prompts");
            }
            return false;
        }

        for (unsigned int i = 0; i < nprompts; ++i) {
            char echo = 0;
            ssh_userauth_kbdint_getprompt(session, i, &echo);
            const char* answer = "";
            if (echo == 0) {
                answer = passUtf8.constData();
            } else if (!userUtf8.isEmpty()) {
                answer = userUtf8.constData();
            }
            if (ssh_userauth_kbdint_setanswer(session, i, answer) != SSH_OK) {
                if (errorOut) {
                    *errorOut = QString::fromUtf8(ssh_get_error(session));
                }
                return false;
            }
        }
    }
}

/**
 * Password auth with keyboard-interactive fallback.
 * Some servers disable ssh-userauth password but accept the same secret via kbdint.
 */
inline bool sshUserauthPasswordFlexible(ssh_session session,
                                        const QString& password,
                                        const QString& username = {},
                                        QString* errorOut = nullptr)
{
    if (!session || password.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("password required");
        }
        return false;
    }

    const QByteArray passUtf8 = password.toUtf8();
    const int passwordRc = ssh_userauth_password(session, nullptr, passUtf8.constData());
    if (passwordRc == SSH_AUTH_SUCCESS) {
        return true;
    }
    const QString passErr = QString::fromUtf8(ssh_get_error(session));

    QString kbdintErr;
    if (sshTryKbdintPassword(session, password, username, &kbdintErr)) {
        return true;
    }

    if (errorOut) {
        *errorOut = QStringLiteral("password auth failed (%1); keyboard-interactive failed (%2)")
                        .arg(passErr.isEmpty() ? QStringLiteral("denied") : passErr,
                             kbdintErr.isEmpty() ? QStringLiteral("denied") : kbdintErr);
    }
    return false;
}
