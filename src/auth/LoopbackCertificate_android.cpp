#include "auth/LoopbackCertificate.h"

/*
 * Android has no public API for minting an X.509 certificate -- KeyPairGenerator
 * produces keys, and everything that wraps one in a certificate is either
 * hidden or tied to the system keystore -- and the OpenSSL shipped in the APK
 * is Qt's TLS backend rather than a library this code links against.
 *
 * So the consent flow does not complete on Android yet. Reported plainly at the
 * point of use rather than left to surface as a TLS handshake failure the user
 * has no way to interpret.
 */
LoopbackCertificate::Pem LoopbackCertificate::loadOrCreate(QString *errorMessage)
{
    if (errorMessage) {
        *errorMessage = QStringLiteral(
            "Connecting a bank is not supported on Android yet: the redirect has to arrive over "
            "HTTPS, and this build cannot create the certificate that needs. Importing a "
            "statement file still works. Note that this device keeps its own database -- "
            "connecting a bank on the desktop app does not populate this one.");
    }
    return {};
}
