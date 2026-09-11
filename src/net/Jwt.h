#pragma once

#include <QByteArray>
#include <QString>

/*!
 * Minimal RS256 JSON Web Token support.
 *
 * Enable Banking authenticates applications with a short-lived RS256 JWT signed
 * by the private key whose certificate was registered in their control panel.
 * Qt has no public RSA signing API, so this wraps OpenSSL's EVP interface
 * directly rather than shelling out to the openssl binary.
 */
namespace Jwt {

//! Base64url without padding, as required by RFC 7515.
QByteArray base64UrlEncode(const QByteArray &data);

/*!
 * Signs an RS256 token.
 *
 * \a privateKeyPem is a PEM-encoded PKCS#1 or PKCS#8 RSA private key.
 * \a keyId becomes the "kid" header. \a ttlSeconds is clamped to the 24 hour
 * maximum the API accepts. Returns an empty string and sets \a errorMessage on
 * failure.
 */
QString signRs256(const QByteArray &privateKeyPem,
                  const QString &keyId,
                  const QString &issuer,
                  const QString &audience,
                  int ttlSeconds,
                  QString *errorMessage = nullptr);

} // namespace Jwt
