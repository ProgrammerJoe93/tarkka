#include "net/Jwt.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include <memory>

namespace {

using BioPtr = std::unique_ptr<BIO, decltype(&BIO_free_all)>;
using KeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using MdCtxPtr = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

QString lastOpenSslError()
{
    const unsigned long code = ERR_get_error();
    if (code == 0)
        return QStringLiteral("unknown OpenSSL error");
    char buffer[256] = {};
    ERR_error_string_n(code, buffer, sizeof(buffer));
    return QString::fromLatin1(buffer);
}

} // namespace

namespace Jwt {

QByteArray base64UrlEncode(const QByteArray &data)
{
    return data.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

QString signRs256(const QByteArray &privateKeyPem,
                  const QString &keyId,
                  const QString &issuer,
                  const QString &audience,
                  int ttlSeconds,
                  QString *errorMessage)
{
    const auto fail = [errorMessage](const QString &message) {
        if (errorMessage)
            *errorMessage = message;
        return QString();
    };

    if (privateKeyPem.isEmpty())
        return fail(QStringLiteral("No private key supplied."));
    if (keyId.isEmpty())
        return fail(QStringLiteral("No application id supplied for the JWT 'kid' header."));

    BioPtr bio(BIO_new_mem_buf(privateKeyPem.constData(), static_cast<int>(privateKeyPem.size())),
               &BIO_free_all);
    if (!bio)
        return fail(QStringLiteral("Out of memory reading the private key."));

    KeyPtr key(PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, nullptr), &EVP_PKEY_free);
    if (!key) {
        return fail(QStringLiteral("Could not read the private key (is it an unencrypted PEM "
                                   "RSA key?): %1")
                        .arg(lastOpenSslError()));
    }
    if (EVP_PKEY_base_id(key.get()) != EVP_PKEY_RSA)
        return fail(QStringLiteral("The private key is not an RSA key; RS256 requires RSA."));

    // The API rejects tokens claiming a lifetime over 24 hours.
    constexpr int maxTtlSeconds = 86400;
    ttlSeconds = qBound(60, ttlSeconds, maxTtlSeconds);

    const qint64 issuedAt = QDateTime::currentSecsSinceEpoch();

    QJsonObject header{{QStringLiteral("typ"), QStringLiteral("JWT")},
                       {QStringLiteral("alg"), QStringLiteral("RS256")},
                       {QStringLiteral("kid"), keyId}};
    QJsonObject payload{{QStringLiteral("iss"), issuer},
                        {QStringLiteral("aud"), audience},
                        {QStringLiteral("iat"), issuedAt},
                        {QStringLiteral("exp"), issuedAt + ttlSeconds}};

    const QByteArray signingInput =
        base64UrlEncode(QJsonDocument(header).toJson(QJsonDocument::Compact)) + '.'
        + base64UrlEncode(QJsonDocument(payload).toJson(QJsonDocument::Compact));

    MdCtxPtr ctx(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
    if (!ctx)
        return fail(QStringLiteral("Out of memory creating a digest context."));

    if (EVP_DigestSignInit(ctx.get(), nullptr, EVP_sha256(), nullptr, key.get()) != 1)
        return fail(QStringLiteral("EVP_DigestSignInit failed: %1").arg(lastOpenSslError()));

    size_t signatureLength = 0;
    if (EVP_DigestSign(ctx.get(),
                       nullptr,
                       &signatureLength,
                       reinterpret_cast<const unsigned char *>(signingInput.constData()),
                       static_cast<size_t>(signingInput.size()))
        != 1) {
        return fail(QStringLiteral("Could not size the signature: %1").arg(lastOpenSslError()));
    }

    QByteArray signature(static_cast<qsizetype>(signatureLength), Qt::Uninitialized);
    if (EVP_DigestSign(ctx.get(),
                       reinterpret_cast<unsigned char *>(signature.data()),
                       &signatureLength,
                       reinterpret_cast<const unsigned char *>(signingInput.constData()),
                       static_cast<size_t>(signingInput.size()))
        != 1) {
        return fail(QStringLiteral("Signing failed: %1").arg(lastOpenSslError()));
    }
    signature.truncate(static_cast<qsizetype>(signatureLength));

    return QString::fromLatin1(signingInput + '.' + base64UrlEncode(signature));
}

} // namespace Jwt
