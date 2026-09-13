#include "auth/LoopbackCertificate.h"
#include "auth/RedirectListener.h"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QElapsedTimer>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QTcpServer>
#include <QStandardPaths>
#include <QTest>

/*!
 * Exercises the loopback redirect the consent flow ends on.
 *
 * The interesting part is the handshake: the listener has to present a
 * certificate a client will complete TLS against, because Enable Banking will
 * only register an https redirect URL. A plain socket here would look fine in
 * every unit test and fail the moment a real browser arrived.
 */
class TestRedirect : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void offersAnHttpsLocalhostUrl();
    void deliversTheCodeOverTls();
    void presentsACertificateValidForLocalhost();
    void reportsAnAuthorisationError();

private:
    //! A free port, so a developer already running the app does not collide.
    static quint16 freePort();
    //! Speaks to the listener the way the browser will, trusting the self-signed pair.
    static QByteArray request(quint16 port, const QString &target, QString *errorMessage);
};

void TestRedirect::initTestCase()
{
    if (!QSslSocket::supportsSsl())
        QSKIP("No TLS backend available in this build.");

    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("tarkka-tests"));
    QCoreApplication::setApplicationName(QStringLiteral("tarkka-tests"));
}

quint16 TestRedirect::freePort()
{
    QTcpServer probe;
    return probe.listen(QHostAddress::LocalHost, 0) ? probe.serverPort() : 0;
}

QByteArray TestRedirect::request(quint16 port, const QString &target, QString *errorMessage)
{
    QSslSocket socket;
    // The certificate is self-signed by design, so a client that refused it
    // would be testing its own trust store rather than this listener.
    socket.setPeerVerifyMode(QSslSocket::VerifyNone);
    socket.connectToHostEncrypted(QStringLiteral("localhost"), port);

    /*
     * qWait() rather than waitForEncrypted(): the listener lives in this same
     * thread, so a blocking wait on the client socket starves the server of
     * the event loop it needs to answer, and the handshake never finishes.
     */
    QElapsedTimer timer;
    timer.start();
    while (!socket.isEncrypted() && timer.elapsed() < 5000) {
        if (socket.state() == QAbstractSocket::UnconnectedState)
            break;
        QTest::qWait(10);
    }
    if (!socket.isEncrypted()) {
        if (errorMessage)
            *errorMessage = socket.errorString();
        return {};
    }

    socket.write(QStringLiteral("GET %1 HTTP/1.1\r\nHost: localhost\r\n\r\n").arg(target).toUtf8());

    QByteArray response;
    timer.restart();
    while (response.isEmpty() && timer.elapsed() < 5000) {
        QTest::qWait(10);
        response += socket.readAll();
    }
    if (response.isEmpty() && errorMessage)
        *errorMessage = QStringLiteral("no response: %1").arg(socket.errorString());
    return response;
}

void TestRedirect::offersAnHttpsLocalhostUrl()
{
    // The control panel rejects the numeric host and any http URL, so these two
    // properties are what make the registered value acceptable at all.
    const QString url = RedirectListener::redirectUrlForPort(8765);
    QCOMPARE(url, QStringLiteral("https://localhost:8765/callback"));
}

void TestRedirect::deliversTheCodeOverTls()
{
    const quint16 port = freePort();
    QVERIFY(port != 0);

    RedirectListener listener;
    QString errorMessage;
    QVERIFY2(listener.start(port, &errorMessage), qPrintable(errorMessage));
    QVERIFY(listener.isListening());

    QSignalSpy received(&listener, &RedirectListener::codeReceived);

    const QByteArray response =
        request(port, QStringLiteral("/callback?code=abc123&state=xyz"), &errorMessage);
    QVERIFY2(!response.isEmpty(), qPrintable(errorMessage));
    QVERIFY(response.startsWith("HTTP/1.1 200"));

    // QTRY_ rather than wait(): request() pumps the event loop, so the signal
    // has usually already arrived by the time we get here and wait() would sit
    // waiting for a second one that never comes.
    QTRY_COMPARE(received.size(), 1);
    QCOMPARE(received.first().at(0).toString(), QStringLiteral("abc123"));
    QCOMPARE(received.first().at(1).toString(), QStringLiteral("xyz"));
}

void TestRedirect::presentsACertificateValidForLocalhost()
{
    const quint16 port = freePort();
    QVERIFY(port != 0);

    RedirectListener listener;
    QString errorMessage;
    QVERIFY2(listener.start(port, &errorMessage), qPrintable(errorMessage));

    /*
     * Verification fully on, with the generated certificate supplied as the
     * only trust anchor -- which is the position a browser is in once the user
     * has accepted the warning. It fails unless the certificate really carries
     * a subjectAltName matching "localhost"; browsers stopped honouring the
     * common name years ago, so a certificate that only sets CN would be
     * rejected outright rather than merely warned about.
     */
    QString error;
    const LoopbackCertificate::Pem pem = LoopbackCertificate::loadOrCreate(&error);
    QVERIFY2(pem.isValid(), qPrintable(error));

    QSslConfiguration configuration = QSslConfiguration::defaultConfiguration();
    configuration.setCaCertificates({QSslCertificate(pem.certificate, QSsl::Pem)});
    QSslSocket socket;
    socket.setSslConfiguration(configuration);
    socket.setPeerVerifyMode(QSslSocket::VerifyPeer);
    socket.setPeerVerifyName(QStringLiteral("localhost"));
    socket.connectToHostEncrypted(QStringLiteral("localhost"), port);

    QElapsedTimer timer;
    timer.start();
    while (!socket.isEncrypted() && timer.elapsed() < 5000
           && socket.state() != QAbstractSocket::UnconnectedState) {
        QTest::qWait(10);
    }
    QVERIFY2(socket.isEncrypted(), qPrintable(socket.errorString()));
}

void TestRedirect::reportsAnAuthorisationError()
{
    const quint16 port = freePort();
    QVERIFY(port != 0);

    RedirectListener listener;
    QString errorMessage;
    QVERIFY2(listener.start(port, &errorMessage), qPrintable(errorMessage));

    QSignalSpy failed(&listener, &RedirectListener::authorisationFailed);

    const QByteArray response = request(
        port,
        QStringLiteral("/callback?error=access_denied&error_description=Consent%20was%20refused"),
        &errorMessage);
    QVERIFY2(!response.isEmpty(), qPrintable(errorMessage));

    QTRY_COMPARE(failed.size(), 1);
    QCOMPARE(failed.first().at(0).toString(), QStringLiteral("Consent was refused"));
}

QTEST_GUILESS_MAIN(TestRedirect)
#include "tst_redirect.moc"
