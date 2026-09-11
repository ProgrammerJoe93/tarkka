#pragma once

#include <QObject>
#include <QString>

class QTcpServer;

/*!
 * Catches the bank's post-consent redirect on a loopback port.
 *
 * The consent flow ends with the browser being sent to the application's
 * registered redirect URL carrying ?code=... . Listening on 127.0.0.1 keeps the
 * whole exchange on this machine; the URL must also be whitelisted in the
 * Enable Banking control panel, which is why the port is configurable rather
 * than ephemeral.
 */
class RedirectListener : public QObject
{
    Q_OBJECT

public:
    explicit RedirectListener(QObject *parent = nullptr);

    //! Starts listening. Returns false and sets \a errorMessage if the port is taken.
    bool start(quint16 port, QString *errorMessage = nullptr);
    void stop();
    bool isListening() const;

    //! The URL to register with the provider, e.g. http://127.0.0.1:8765/callback.
    static QString redirectUrlForPort(quint16 port);

signals:
    //! Emitted once the browser hits the redirect URL with an authorisation code.
    void codeReceived(const QString &code, const QString &state);
    //! Emitted when the redirect carried an error instead of a code.
    void authorisationFailed(const QString &message);

private:
    void handleConnection();

    QTcpServer *m_server;
};
