#pragma once

#include <QByteArray>
#include <QString>

/*!
 * The TLS material the redirect listener serves on loopback.
 *
 * Enable Banking's control panel rejects an \c http redirect URL outright, so
 * the listener the bank redirects a browser back to has to speak TLS even
 * though it never leaves the machine. Nobody issues certificates for
 * "localhost", so the app signs its own.
 *
 * The browser will warn the first time -- a self-signed certificate is exactly
 * what a machine-in-the-middle would present, and the browser cannot tell the
 * difference. The pair is kept on disk rather than regenerated per run so the
 * exception a user grants once continues to hold.
 */
namespace LoopbackCertificate {

struct Pem
{
    QByteArray certificate;
    QByteArray privateKey;

    bool isValid() const { return !certificate.isEmpty() && !privateKey.isEmpty(); }
};

/*!
 * Returns the stored pair, creating it on first use.
 *
 * Returns an invalid Pem and sets \a errorMessage if the pair can neither be
 * read nor created.
 */
Pem loadOrCreate(QString *errorMessage = nullptr);

} // namespace LoopbackCertificate
