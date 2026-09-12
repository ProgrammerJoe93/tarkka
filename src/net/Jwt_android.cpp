#include "net/Jwt.h"
#include "net/JwtKey.h"

#include <QDateTime>
#include <QJniEnvironment>
#include <QJniObject>
#include <QJsonDocument>
#include <QJsonObject>

/*
 * RS256 on Android.
 *
 * Qt for Android ships no OpenSSL, and bundling a copy would mean shipping a
 * crypto library this app then has to keep patched. Android already provides
 * SHA256withRSA through the platform JCA, so this signs through that instead --
 * no extra shared object, and the implementation is maintained by the OS.
 */
namespace {

//! Clears and reports any pending JVM exception. True if one was pending.
bool takeJniException(QString *errorMessage, const QString &context)
{
    QJniEnvironment env;
    if (!env.checkAndClearExceptions())
        return false;
    if (errorMessage)
        *errorMessage = QStringLiteral("Android rejected the key while %1.").arg(context);
    return true;
}

QJniObject toJavaBytes(const QByteArray &data)
{
    QJniEnvironment env;
    jbyteArray array = env->NewByteArray(static_cast<jsize>(data.size()));
    if (!array)
        return {};
    env->SetByteArrayRegion(array, 0, static_cast<jsize>(data.size()),
                            reinterpret_cast<const jbyte *>(data.constData()));
    return QJniObject(array);
}

QByteArray fromJavaBytes(const QJniObject &object)
{
    if (!object.isValid())
        return {};

    QJniEnvironment env;
    jbyteArray array = object.object<jbyteArray>();
    const jsize length = env->GetArrayLength(array);
    QByteArray result(static_cast<qsizetype>(length), Qt::Uninitialized);
    env->GetByteArrayRegion(array, 0, length, reinterpret_cast<jbyte *>(result.data()));
    return result;
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

    if (keyId.isEmpty())
        return fail(QStringLiteral("No application id supplied for the JWT 'kid' header."));

    QString keyError;
    const QByteArray der = JwtKey::toPkcs8Der(privateKeyPem, &keyError);
    if (der.isEmpty())
        return fail(keyError);

    const QJniObject derBytes = toJavaBytes(der);
    if (!derBytes.isValid())
        return fail(QStringLiteral("Out of memory passing the key to Android."));

    QJniObject keySpec("java/security/spec/PKCS8EncodedKeySpec", "([B)V", derBytes.object());
    if (takeJniException(errorMessage, QStringLiteral("reading it")) || !keySpec.isValid())
        return {};

    const QJniObject keyFactory = QJniObject::callStaticObjectMethod(
        "java/security/KeyFactory", "getInstance",
        "(Ljava/lang/String;)Ljava/security/KeyFactory;",
        QJniObject::fromString(QStringLiteral("RSA")).object<jstring>());
    if (takeJniException(errorMessage, QStringLiteral("preparing RSA")) || !keyFactory.isValid())
        return {};

    const QJniObject privateKey = keyFactory.callObjectMethod(
        "generatePrivate", "(Ljava/security/spec/KeySpec;)Ljava/security/PrivateKey;",
        keySpec.object());
    if (takeJniException(errorMessage, QStringLiteral("decoding it")) || !privateKey.isValid())
        return fail(QStringLiteral("Android could not read the private key. It must be an "
                                   "unencrypted RSA key in PEM form."));

    // The API rejects tokens claiming a lifetime over 24 hours.
    constexpr int maxTtlSeconds = 86400;
    ttlSeconds = qBound(60, ttlSeconds, maxTtlSeconds);
    const qint64 issuedAt = QDateTime::currentSecsSinceEpoch();

    const QJsonObject header{{QStringLiteral("typ"), QStringLiteral("JWT")},
                             {QStringLiteral("alg"), QStringLiteral("RS256")},
                             {QStringLiteral("kid"), keyId}};
    const QJsonObject payload{{QStringLiteral("iss"), issuer},
                              {QStringLiteral("aud"), audience},
                              {QStringLiteral("iat"), issuedAt},
                              {QStringLiteral("exp"), issuedAt + ttlSeconds}};

    const QByteArray signingInput =
        base64UrlEncode(QJsonDocument(header).toJson(QJsonDocument::Compact)) + '.'
        + base64UrlEncode(QJsonDocument(payload).toJson(QJsonDocument::Compact));

    QJniObject signature = QJniObject::callStaticObjectMethod(
        "java/security/Signature", "getInstance",
        "(Ljava/lang/String;)Ljava/security/Signature;",
        QJniObject::fromString(QStringLiteral("SHA256withRSA")).object<jstring>());
    if (takeJniException(errorMessage, QStringLiteral("preparing SHA256withRSA"))
        || !signature.isValid()) {
        return {};
    }

    signature.callMethod<void>("initSign", "(Ljava/security/PrivateKey;)V", privateKey.object());
    if (takeJniException(errorMessage, QStringLiteral("starting the signature")))
        return {};

    signature.callMethod<void>("update", "([B)V", toJavaBytes(signingInput).object());
    if (takeJniException(errorMessage, QStringLiteral("hashing the token")))
        return {};

    const QJniObject signed_ = signature.callObjectMethod("sign", "()[B");
    if (takeJniException(errorMessage, QStringLiteral("signing the token")) || !signed_.isValid())
        return fail(QStringLiteral("Android could not sign the token."));

    return QString::fromLatin1(signingInput + '.' + base64UrlEncode(fromJavaBytes(signed_)));
}

} // namespace Jwt
