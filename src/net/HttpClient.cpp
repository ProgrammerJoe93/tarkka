#include "net/HttpClient.h"

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include <chrono>

using namespace std::chrono_literals;

namespace {

/*!
 * Pulls a useful message out of an API error body.
 *
 * Enable Banking reports problems as {"message": ...} or {"error": ...} and
 * occasionally nests detail under "detail"; fall back to the raw body so we
 * never swallow an unrecognised shape silently.
 */
QString describeErrorBody(const QByteArray &raw)
{
    const QJsonDocument document = QJsonDocument::fromJson(raw);
    if (document.isObject()) {
        const QJsonObject object = document.object();
        for (const auto &key : {"message", "error", "detail", "error_description"}) {
            const QJsonValue value = object.value(QLatin1String(key));
            if (value.isString() && !value.toString().isEmpty())
                return value.toString();
        }
    }
    const QString text = QString::fromUtf8(raw).simplified();
    return text.isEmpty() ? QString() : text.left(400);
}

} // namespace

HttpClient::HttpClient(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
{
    m_requests.setTransferTimeout(30s);
}

void HttpClient::setBaseUrl(const QUrl &baseUrl)
{
    m_requests.setBaseUrl(baseUrl);
}

void HttpClient::setBearerToken(const QByteArray &token)
{
    if (token.isEmpty())
        m_requests.clearBearerToken();
    else
        m_requests.setBearerToken(token);
}

void HttpClient::get(const QString &path, const QUrlQuery &query, Handler handler)
{
    QNetworkRequest request = query.isEmpty() ? m_requests.createRequest(path)
                                              : m_requests.createRequest(path, query);
    finish(m_manager->get(request), handler);
}

void HttpClient::post(const QString &path, const QJsonDocument &body, Handler handler)
{
    QNetworkRequest request = m_requests.createRequest(path);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/json"));
    finish(m_manager->post(request, body.toJson(QJsonDocument::Compact)), handler);
}

void HttpClient::finish(QNetworkReply *reply, const Handler &handler)
{
    connect(reply, &QNetworkReply::finished, this, [reply, handler] {
        reply->deleteLater();

        Response response;
        response.httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        const QByteArray raw = reply->readAll();
        QJsonParseError parseError;
        response.body = QJsonDocument::fromJson(raw, &parseError);

        if (reply->error() != QNetworkReply::NoError) {
            const QString detail = describeErrorBody(raw);
            response.errorMessage = detail.isEmpty()
                                        ? reply->errorString()
                                        : QStringLiteral("%1 (HTTP %2)")
                                              .arg(detail)
                                              .arg(response.httpStatus);
            if (handler)
                handler(response);
            return;
        }

        if (parseError.error != QJsonParseError::NoError) {
            response.errorMessage = QStringLiteral("The server returned a body that is not JSON: %1")
                                        .arg(parseError.errorString());
            if (handler)
                handler(response);
            return;
        }

        response.ok = true;
        if (handler)
            handler(response);
    });
}
