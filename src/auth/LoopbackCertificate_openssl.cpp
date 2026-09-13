#include "auth/LoopbackCertificate.h"

#include "auth/Settings.h"

#include <QFile>
#include <QFileDevice>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>

#include <memory>

namespace {

using BioPtr = std::unique_ptr<BIO, decltype(&BIO_free_all)>;
using KeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using KeyCtxPtr = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;
using X509Ptr = std::unique_ptr<X509, decltype(&X509_free)>;
using ExtPtr = std::unique_ptr<X509_EXTENSION, decltype(&X509_EXTENSION_free)>;

//! Ten years: this certificate is only ever trusted by hand, so an expiry
//! would do nothing but make the user grant the same exception again.
constexpr long kValiditySeconds = 10L * 365 * 24 * 3600;

QString lastOpenSslError()
{
    const unsigned long code = ERR_get_error();
    if (code == 0)
        return QStringLiteral("unknown OpenSSL error");
    char buffer[256] = {};
    ERR_error_string_n(code, buffer, sizeof(buffer));
    return QString::fromLatin1(buffer);
}

KeyPtr generateKey()
{
    KeyCtxPtr ctx(EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr), &EVP_PKEY_CTX_free);
    if (!ctx || EVP_PKEY_keygen_init(ctx.get()) != 1)
        return {nullptr, &EVP_PKEY_free};
    if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx.get(), 2048) != 1)
        return {nullptr, &EVP_PKEY_free};

    EVP_PKEY *raw = nullptr;
    if (EVP_PKEY_keygen(ctx.get(), &raw) != 1)
        return {nullptr, &EVP_PKEY_free};
    return {raw, &EVP_PKEY_free};
}

X509Ptr selfSign(EVP_PKEY *key)
{
    X509Ptr certificate(X509_new(), &X509_free);
    if (!certificate)
        return {nullptr, &X509_free};

    X509_set_version(certificate.get(), 2);  // v3, which is what SANs require.
    ASN1_INTEGER_set(X509_get_serialNumber(certificate.get()), 1);
    // Backdated an hour so a clock that is slightly behind does not reject a
    // certificate generated moments earlier.
    X509_gmtime_adj(X509_getm_notBefore(certificate.get()), -3600);
    X509_gmtime_adj(X509_getm_notAfter(certificate.get()), kValiditySeconds);

    if (X509_set_pubkey(certificate.get(), key) != 1)
        return {nullptr, &X509_free};

    X509_NAME *subject = X509_get_subject_name(certificate.get());
    X509_NAME_add_entry_by_txt(subject, "CN", MBSTRING_ASC,
                               reinterpret_cast<const unsigned char *>("localhost"), -1, -1, 0);
    // Self-signed: the issuer is the subject.
    if (X509_set_issuer_name(certificate.get(), subject) != 1)
        return {nullptr, &X509_free};

    /*
     * Modern browsers ignore the common name entirely and match the host
     * against subjectAltName, so without this the certificate is rejected
     * outright rather than merely warned about. Both spellings of loopback are
     * listed so the same file keeps working if the redirect URL ever goes back
     * to the numeric form.
     */
    X509V3_CTX context;
    X509V3_set_ctx_nodb(&context);
    X509V3_set_ctx(&context, certificate.get(), certificate.get(), nullptr, nullptr, 0);
    ExtPtr san(X509V3_EXT_conf_nid(nullptr, &context, NID_subject_alt_name,
                                   "DNS:localhost,IP:127.0.0.1"),
               &X509_EXTENSION_free);
    if (!san || X509_add_ext(certificate.get(), san.get(), -1) != 1)
        return {nullptr, &X509_free};

    if (X509_sign(certificate.get(), key, EVP_sha256()) == 0)
        return {nullptr, &X509_free};
    return certificate;
}

QByteArray bioContents(BIO *bio)
{
    char *data = nullptr;
    const long size = BIO_get_mem_data(bio, &data);
    return size > 0 ? QByteArray(data, static_cast<qsizetype>(size)) : QByteArray();
}

//! Reads back a file written by a previous run; empty if there is not one.
LoopbackCertificate::Pem readStored(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    const QByteArray contents = file.readAll();
    const qsizetype certificateStart = contents.indexOf("-----BEGIN CERTIFICATE-----");
    if (certificateStart < 0)
        return {};

    return {contents.mid(certificateStart), contents.left(certificateStart)};
}

} // namespace

LoopbackCertificate::Pem LoopbackCertificate::loadOrCreate(QString *errorMessage)
{
    const QString path = Settings::loopbackCertificatePath();

    const Pem stored = readStored(path);
    if (stored.isValid())
        return stored;

    const KeyPtr key = generateKey();
    if (!key) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not generate a key for the redirect listener: %1")
                                .arg(lastOpenSslError());
        }
        return {};
    }

    const X509Ptr certificate = selfSign(key.get());
    if (!certificate) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not create the redirect listener's "
                                           "certificate: %1")
                                .arg(lastOpenSslError());
        }
        return {};
    }

    BioPtr keyBio(BIO_new(BIO_s_mem()), &BIO_free_all);
    BioPtr certificateBio(BIO_new(BIO_s_mem()), &BIO_free_all);
    if (!keyBio || !certificateBio
        || PEM_write_bio_PrivateKey(keyBio.get(), key.get(), nullptr, nullptr, 0, nullptr, nullptr)
               != 1
        || PEM_write_bio_X509(certificateBio.get(), certificate.get()) != 1) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not encode the redirect listener's "
                                           "certificate: %1")
                                .arg(lastOpenSslError());
        }
        return {};
    }

    const Pem pem{bioContents(certificateBio.get()), bioContents(keyBio.get())};
    if (!pem.isValid()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("The generated certificate came out empty.");
        return {};
    }

    // Best effort: a pair that cannot be stored still works for this run, it
    // just means the browser asks about a new certificate next time. Failing
    // the whole consent flow over it would be worse.
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        file.write(pem.privateKey);
        file.write(pem.certificate);
    }
    return pem;
}
