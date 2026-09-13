#pragma once

#include <QObject>
#include <QString>

class QSslServer;

/*!
 * Catches the bank's post-consent redirect on a loopback port.
 *
 * The consent flow ends with the browser being sent to the application's
 * registered redirect URL carrying ?code=... . Listening on loopback keeps the
 * whole exchange on this machine; the URL must also be whitelisted in the
 * Enable Banking control panel, which is why the port is configurable rather
 * than ephemeral.
 *
 * The connection is TLS even though it never leaves the machine, because the
 * control panel will not register an http redirect URL. See
 * LoopbackCertificate for the certificate that entails.
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

    /*!
     * The URL to register with the provider, e.g. https://localhost:8765/callback.
     *
     * HTTPS, and "localhost" rather than 127.0.0.1, because Enable Banking's
     * control panel rejects the numeric host as an invalid URL and plain http
     * as an unsupported scheme -- stricter than RFC 8252, which recommends
     * exactly the loopback http form for native apps.
     */
    static QString redirectUrlForPort(quint16 port);

signals:
    //! Emitted once the browser hits the redirect URL with an authorisation code.
    void codeReceived(const QString &code, const QString &state);
    //! Emitted when the redirect carried an error instead of a code.
    void authorisationFailed(const QString &message);

private:
    void handleConnection();

    QSslServer *m_server;
};
