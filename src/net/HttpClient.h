#pragma once

#include <QJsonDocument>
#include <QNetworkRequestFactory>
#include <QObject>
#include <QUrlQuery>

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

/*!
 * Thin JSON-over-HTTPS helper around QNetworkAccessManager.
 *
 * Callers get the parsed body or a human-readable error; the API's own error
 * payload is unpacked into that message so failures say what the bank actually
 * complained about rather than just "Protocol error".
 */
class HttpClient : public QObject
{
    Q_OBJECT

public:
    struct Response
    {
        bool ok = false;
        int httpStatus = 0;
        QJsonDocument body;
        QString errorMessage;
    };

    using Handler = std::function<void(const Response &)>;

    explicit HttpClient(QObject *parent = nullptr);

    void setBaseUrl(const QUrl &baseUrl);
    void setBearerToken(const QByteArray &token);
    /*!
     * Sets a raw Authorization header.
     *
     * Not every API uses the Bearer scheme -- Trading 212 expects the bare API
     * key -- so this bypasses the prefix setBearerToken adds.
     */
    void setAuthorizationHeader(const QByteArray &value);

    void get(const QString &path, const QUrlQuery &query, Handler handler);
    void post(const QString &path, const QJsonDocument &body, Handler handler);

private:
    void finish(QNetworkReply *reply, const Handler &handler);

    QNetworkAccessManager *m_manager;
    QNetworkRequestFactory m_requests;
};
