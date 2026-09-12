#include "auth/RedirectListener.h"

#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>

namespace {

QByteArray htmlResponse(const QString &title, const QString &body)
{
    const QByteArray html = QStringLiteral(
                                "<!doctype html><html><head><meta charset=\"utf-8\">"
                                "<title>%1</title><style>"
                                "body{font:16px system-ui,sans-serif;background:#12141a;color:#e7e9ee;"
                                "display:grid;place-items:center;height:100vh;margin:0}"
                                "div{text-align:center;max-width:32rem;padding:2rem}"
                                "h1{font-size:1.25rem;margin:0 0 .5rem}"
                                "p{color:#9aa1b1;margin:0}"
                                "</style></head><body><div><h1>%1</h1><p>%2</p></div></body></html>")
                                .arg(title, body)
                                .toUtf8();

    return "HTTP/1.1 200 OK\r\n"
           "Content-Type: text/html; charset=utf-8\r\n"
           "Content-Length: " + QByteArray::number(html.size()) + "\r\n"
           "Connection: close\r\n"
           "\r\n" + html;
}

} // namespace

RedirectListener::RedirectListener(QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection, this, &RedirectListener::handleConnection);
}

QString RedirectListener::redirectUrlForPort(quint16 port)
{
    return QStringLiteral("http://127.0.0.1:%1/callback").arg(port);
}

bool RedirectListener::start(quint16 port, QString *errorMessage)
{
    if (m_server->isListening())
        return true;

    if (!m_server->listen(QHostAddress::LocalHost, port)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Cannot listen on 127.0.0.1:%1 for the bank's redirect: %2")
                                .arg(port)
                                .arg(m_server->errorString());
        }
        return false;
    }
    return true;
}

void RedirectListener::stop()
{
    m_server->close();
}

bool RedirectListener::isListening() const
{
    return m_server->isListening();
}

void RedirectListener::handleConnection()
{
    while (QTcpSocket *socket = m_server->nextPendingConnection()) {
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
            const QByteArray request = socket->readAll();
            const qsizetype lineEnd = request.indexOf("\r\n");
            const QByteArray requestLine = request.left(lineEnd < 0 ? request.size() : lineEnd);

            // "GET /callback?code=...&state=... HTTP/1.1"
            const QList<QByteArray> parts = requestLine.split(' ');
            if (parts.size() < 2) {
                socket->write(htmlResponse(tr("Unexpected request"),
                                           tr("The browser sent something this listener could "
                                              "not read.")));
                socket->disconnectFromHost();
                return;
            }

            const QUrlQuery query(QUrl(QString::fromUtf8(parts.at(1))).query());
            const QString code = query.queryItemValue(QStringLiteral("code"));
            const QString state = query.queryItemValue(QStringLiteral("state"));
            const QString error = query.queryItemValue(QStringLiteral("error"));

            if (!code.isEmpty()) {
                socket->write(htmlResponse(tr("Bank connected"),
                                           tr("You can close this tab and return to Tarkka.")));
                socket->disconnectFromHost();
                emit codeReceived(code, state);
                return;
            }

            if (!error.isEmpty()) {
                const QString description =
                    query.queryItemValue(QStringLiteral("error_description"));
                socket->write(htmlResponse(tr("Authorisation failed"),
                                           description.isEmpty() ? error : description));
                socket->disconnectFromHost();
                emit authorisationFailed(description.isEmpty() ? error : description);
                return;
            }

            // Browsers routinely ask for /favicon.ico on the same origin; that
            // is not a failed consent, so answer it without reporting anything.
            socket->write(htmlResponse(tr("Waiting for the bank"),
                                       tr("This page is the redirect target for Tarkka.")));
            socket->disconnectFromHost();
        });
    }
}
