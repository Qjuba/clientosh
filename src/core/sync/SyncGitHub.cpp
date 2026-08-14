#include "SyncGitHub.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace {
const char* kGistApiBase = "https://api.github.com/gists";
} // namespace

bool SyncGitHub::perform(const QByteArray& method, const QString& url,
                         const QString& token, const QByteArray& payload,
                         int* httpStatusOut, QByteArray* responseBodyOut,
                         QByteArray& errorOut)
{
    QNetworkAccessManager nam;
    QUrl qurl(url);
    QNetworkRequest req(qurl);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("clientosh-sync/1.0"));
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    req.setRawHeader("Authorization",
                     QByteArray("Bearer ") + token.toUtf8());

    QNetworkReply* reply = nullptr;
    if (method == "POST") {
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        reply = nam.post(req, payload);
    } else if (method == "PATCH") {
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        reply = nam.sendCustomRequest(req, "PATCH", payload);
    } else { // GET
        reply = nam.get(req);
    }

    // Block in a worker thread until the reply arrives. (Never call from GUI.)
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray resp = reply->readAll();
    const bool ok = (reply->error() == QNetworkReply::NoError
                     && status >= 200 && status < 300);

    if (httpStatusOut) {
        *httpStatusOut = status;
    }
    if (responseBodyOut) {
        *responseBodyOut = resp;
    }

    if (!ok) {
        const QString reason = reply->errorString();
        errorOut = reason.toUtf8();
    }
    reply->deleteLater();
    return ok;
}

SyncGitHub::CreateResult SyncGitHub::createGist(const QString& token,
                                                const QString& description,
                                                const QString& filename,
                                                const QByteArray& body)
{
    CreateResult result;

    QJsonObject files;
    files.insert(filename, QJsonObject{{QStringLiteral("content"),
                                        QString::fromUtf8(body)}});
    QJsonObject reqObj;
    reqObj.insert(QStringLiteral("description"), description);
    reqObj.insert(QStringLiteral("public"), false);
    reqObj.insert(QStringLiteral("files"), files);
    const QByteArray payload = QJsonDocument(reqObj).toJson(QJsonDocument::Compact);

    int status = 0;
    QByteArray resp;
    QByteArray error;
    const bool ok = perform("POST", QLatin1String(kGistApiBase), token, payload,
                            &status, &resp, error);
    if (!ok) {
        result.error = QString::fromUtf8(error);
        return result;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(resp);
    if (doc.isObject()) {
        result.gistId = doc.object().value(QStringLiteral("id")).toString();
    }
    result.ok = !result.gistId.isEmpty();
    if (!result.ok) {
        result.error = QStringLiteral("GitHub did not return a gist id");
    }
    return result;
}

bool SyncGitHub::checkToken(const QString& token, QString* errorOut)
{
    int status = 0;
    QByteArray resp;
    QByteArray error;
    const bool ok = perform("GET", QLatin1String("https://api.github.com/user"),
                            token, QByteArray(), &status, &resp, error);
    if (!ok && errorOut) {
        *errorOut = QString::fromUtf8(error);
    }
    return ok;
}

SyncGitHub::WriteResult SyncGitHub::updateGist(const QString& token,
                                               const QString& gistId,
                                               const QString& filename,
                                               const QByteArray& body)
{
    WriteResult result;

    QJsonObject files;
    files.insert(filename, QJsonObject{{QStringLiteral("content"),
                                        QString::fromUtf8(body)}});
    QJsonObject reqObj;
    reqObj.insert(QStringLiteral("files"), files);
    const QByteArray payload = QJsonDocument(reqObj).toJson(QJsonDocument::Compact);

    const QString url = QStringLiteral("%1/%2").arg(QLatin1String(kGistApiBase), gistId);
    int status = 0;
    QByteArray resp;
    QByteArray error;
    const bool ok = perform("PATCH", url, token, payload, &status, &resp, error);
    result.ok = ok;
    if (!ok) {
        result.error = QString::fromUtf8(error);
    }
    return result;
}

SyncGitHub::ReadResult SyncGitHub::readGist(const QString& token,
                                            const QString& gistId,
                                            const QString& filename)
{
    ReadResult result;

    const QString url = QStringLiteral("%1/%2").arg(QLatin1String(kGistApiBase), gistId);
    int status = 0;
    QByteArray resp;
    QByteArray error;
    const bool ok = perform("GET", url, token, QByteArray(), &status, &resp, error);
    if (!ok) {
        if (status == 404) {
            result.notFound = true;
            result.error = QStringLiteral("Gist not found (id %1)").arg(gistId);
        } else {
            result.error = QString::fromUtf8(error);
        }
        return result;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(resp);
    const QJsonObject files = doc.object().value(QStringLiteral("files")).toObject();
    const QJsonObject file = files.value(filename).toObject();
    if (!file.isEmpty()) {
        result.body = file.value(QStringLiteral("content")).toString().toUtf8();
        result.ok = true;
        return result;
    }

    // Fallback: accept the whole first file (gist rename resilience).
    for (const QJsonValue& v : files) {
        const QJsonObject f = v.toObject();
        if (f.contains(QStringLiteral("content"))) {
            result.body = f.value(QStringLiteral("content")).toString().toUtf8();
            result.ok = true;
            return result;
        }
    }

    result.error = QStringLiteral("Sync file not found in gist");
    return result;
}
