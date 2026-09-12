#pragma once

#include <QByteArray>
#include <QString>

/*!
 * Private key decoding, shared by every platform's signer.
 *
 * Kept separate from the signing back ends because the awkward part -- working
 * out which of the two PEM shapes a key is in and normalising it -- is pure
 * data handling and can be tested on the desktop, where the Android back end
 * cannot be.
 */
namespace JwtKey {

/*!
 * Converts a PEM private key to PKCS#8 DER.
 *
 * Accepts both encodings openssl produces: PKCS#8 ("BEGIN PRIVATE KEY", what
 * `openssl genpkey` writes) is decoded directly, and PKCS#1 ("BEGIN RSA PRIVATE
 * KEY", what `openssl genrsa` writes) is wrapped in the PKCS#8 envelope. Java's
 * KeyFactory only accepts PKCS#8, so without the wrapping half of the keys a
 * user might reasonably generate would be rejected on Android.
 *
 * Returns an empty array and sets \a errorMessage on failure.
 */
QByteArray toPkcs8Der(const QByteArray &pem, QString *errorMessage = nullptr);

//! True if \a pem carries a PKCS#1 ("BEGIN RSA PRIVATE KEY") body.
bool isPkcs1(const QByteArray &pem);

} // namespace JwtKey
