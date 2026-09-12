#include "net/JwtKey.h"

#include <QRegularExpression>

namespace {

//! DER length octets for \a length, short or long form as required.
QByteArray derLength(qsizetype length)
{
    if (length < 0x80)
        return QByteArray(1, static_cast<char>(length));

    QByteArray bytes;
    for (qsizetype value = length; value > 0; value >>= 8)
        bytes.prepend(static_cast<char>(value & 0xFF));
    return QByteArray(1, static_cast<char>(0x80 | bytes.size())) + bytes;
}

QByteArray derElement(quint8 tag, const QByteArray &contents)
{
    return QByteArray(1, static_cast<char>(tag)) + derLength(contents.size()) + contents;
}

//! Strips the PEM armour and decodes the base64 body.
QByteArray decodeBody(const QByteArray &pem, QString *errorMessage)
{
    static const QRegularExpression armour(
        QStringLiteral("-----(BEGIN|END)[^-]*-----"));

    QString text = QString::fromLatin1(pem);
    text.remove(armour);
    text.remove(QLatin1Char('\r'));
    text.remove(QLatin1Char('\n'));
    text.remove(QLatin1Char(' '));
    text.remove(QLatin1Char('\t'));

    const QByteArray::FromBase64Result decoded =
        QByteArray::fromBase64Encoding(text.toLatin1(), QByteArray::Base64Encoding
                                                            | QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded) {
        if (errorMessage)
            *errorMessage = QStringLiteral("The private key is not valid base64 PEM.");
        return {};
    }
    return *decoded;
}

} // namespace

namespace JwtKey {

bool isPkcs1(const QByteArray &pem)
{
    return pem.contains("BEGIN RSA PRIVATE KEY");
}

QByteArray toPkcs8Der(const QByteArray &pem, QString *errorMessage)
{
    if (pem.trimmed().isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("No private key supplied.");
        return {};
    }
    if (pem.contains("ENCRYPTED")) {
        if (errorMessage)
            *errorMessage = QStringLiteral("The private key is encrypted; supply an "
                                           "unencrypted PEM key.");
        return {};
    }

    const QByteArray body = decodeBody(pem, errorMessage);
    if (body.isEmpty())
        return {};

    if (!isPkcs1(pem))
        return body;  // Already PKCS#8.

    // PKCS#8 ::= SEQUENCE { version INTEGER 0,
    //                       algorithm SEQUENCE { OID rsaEncryption, NULL },
    //                       privateKey OCTET STRING (the PKCS#1 body) }
    static const QByteArray rsaEncryptionOid =
        QByteArray::fromHex("06092a864886f70d010101");  // 1.2.840.113549.1.1.1
    const QByteArray algorithm =
        derElement(0x30, rsaEncryptionOid + QByteArray::fromHex("0500"));
    const QByteArray version = QByteArray::fromHex("020100");

    return derElement(0x30, version + algorithm + derElement(0x04, body));
}

} // namespace JwtKey
