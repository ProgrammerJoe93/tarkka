#include "net/Jwt.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

class TestJwt : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void base64UrlHasNoPaddingOrSlashes();
    void producesThreeSegments();
    void headerAndPayloadCarryTheRightClaims();
    void clampsTokenLifetime();
    void rejectsAMissingKey();
    void rejectsANonRsaKey();
    void signatureVerifiesAgainstThePublicKey();

private:
    QTemporaryDir m_directory;
    QByteArray m_privateKey;
    QString m_publicKeyPath;
};

void TestJwt::initTestCase()
{
    QVERIFY(m_directory.isValid());

    const QString privatePath = m_directory.filePath(QStringLiteral("private.pem"));
    m_publicKeyPath = m_directory.filePath(QStringLiteral("public.pem"));

    QProcess generate;
    generate.start(QStringLiteral("openssl"),
                   {"genpkey", "-algorithm", "RSA", "-pkeyopt", "rsa_keygen_bits:2048",
                    "-out", privatePath});
    QVERIFY2(generate.waitForFinished(30000) && generate.exitCode() == 0,
             "openssl is required to generate a test key pair");

    QProcess extract;
    extract.start(QStringLiteral("openssl"),
                  {"rsa", "-in", privatePath, "-pubout", "-out", m_publicKeyPath});
    QVERIFY(extract.waitForFinished(30000) && extract.exitCode() == 0);

    QFile file(privatePath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    m_privateKey = file.readAll();
    QVERIFY(!m_privateKey.isEmpty());
}

void TestJwt::base64UrlHasNoPaddingOrSlashes()
{
    const QByteArray encoded = Jwt::base64UrlEncode(QByteArray::fromHex("fbff3e"));
    QVERIFY(!encoded.contains('='));
    QVERIFY(!encoded.contains('+'));
    QVERIFY(!encoded.contains('/'));
}

void TestJwt::producesThreeSegments()
{
    QString errorMessage;
    const QString token = Jwt::signRs256(m_privateKey, QStringLiteral("app-id"),
                                         QStringLiteral("enablebanking.com"),
                                         QStringLiteral("api.enablebanking.com"),
                                         3600, &errorMessage);
    QVERIFY2(!token.isEmpty(), qPrintable(errorMessage));
    QCOMPARE(token.count(QLatin1Char('.')), 2);
}

void TestJwt::headerAndPayloadCarryTheRightClaims()
{
    const QString token = Jwt::signRs256(m_privateKey, QStringLiteral("app-id"),
                                         QStringLiteral("enablebanking.com"),
                                         QStringLiteral("api.enablebanking.com"),
                                         3600, nullptr);
    const QStringList parts = token.split(QLatin1Char('.'));
    QCOMPARE(parts.size(), 3);

    const QJsonObject header = QJsonDocument::fromJson(
        QByteArray::fromBase64(parts.at(0).toLatin1(), QByteArray::Base64UrlEncoding)).object();
    QCOMPARE(header.value("alg").toString(), QStringLiteral("RS256"));
    QCOMPARE(header.value("typ").toString(), QStringLiteral("JWT"));
    QCOMPARE(header.value("kid").toString(), QStringLiteral("app-id"));

    const QJsonObject payload = QJsonDocument::fromJson(
        QByteArray::fromBase64(parts.at(1).toLatin1(), QByteArray::Base64UrlEncoding)).object();
    QCOMPARE(payload.value("iss").toString(), QStringLiteral("enablebanking.com"));
    QCOMPARE(payload.value("aud").toString(), QStringLiteral("api.enablebanking.com"));
    QCOMPARE(payload.value("exp").toInteger() - payload.value("iat").toInteger(), 3600LL);
}

void TestJwt::clampsTokenLifetime()
{
    // The API rejects anything claiming more than 24 hours.
    const QString token = Jwt::signRs256(m_privateKey, QStringLiteral("app-id"),
                                         QStringLiteral("enablebanking.com"),
                                         QStringLiteral("api.enablebanking.com"),
                                         999999, nullptr);
    const QStringList parts = token.split(QLatin1Char('.'));
    const QJsonObject payload = QJsonDocument::fromJson(
        QByteArray::fromBase64(parts.at(1).toLatin1(), QByteArray::Base64UrlEncoding)).object();
    QCOMPARE(payload.value("exp").toInteger() - payload.value("iat").toInteger(), 86400LL);
}

void TestJwt::rejectsAMissingKey()
{
    QString errorMessage;
    QVERIFY(Jwt::signRs256({}, QStringLiteral("app"), QStringLiteral("i"), QStringLiteral("a"),
                           600, &errorMessage)
                .isEmpty());
    QVERIFY(!errorMessage.isEmpty());
}

void TestJwt::rejectsANonRsaKey()
{
    const QString path = m_directory.filePath(QStringLiteral("ec.pem"));
    QProcess generate;
    generate.start(QStringLiteral("openssl"),
                   {"genpkey", "-algorithm", "EC", "-pkeyopt", "ec_paramgen_curve:P-256",
                    "-out", path});
    QVERIFY(generate.waitForFinished(30000) && generate.exitCode() == 0);

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));

    QString errorMessage;
    QVERIFY(Jwt::signRs256(file.readAll(), QStringLiteral("app"), QStringLiteral("i"),
                           QStringLiteral("a"), 600, &errorMessage)
                .isEmpty());
    QVERIFY(errorMessage.contains(QStringLiteral("RSA")));
}

void TestJwt::signatureVerifiesAgainstThePublicKey()
{
    const QString token = Jwt::signRs256(m_privateKey, QStringLiteral("app-id"),
                                         QStringLiteral("enablebanking.com"),
                                         QStringLiteral("api.enablebanking.com"),
                                         3600, nullptr);
    const QStringList parts = token.split(QLatin1Char('.'));
    QCOMPARE(parts.size(), 3);

    const QString signingInput = parts.at(0) + QLatin1Char('.') + parts.at(1);
    const QByteArray signature =
        QByteArray::fromBase64(parts.at(2).toLatin1(), QByteArray::Base64UrlEncoding);

    const QString dataPath = m_directory.filePath(QStringLiteral("signed.txt"));
    const QString signaturePath = m_directory.filePath(QStringLiteral("signature.bin"));

    QFile dataFile(dataPath);
    QVERIFY(dataFile.open(QIODevice::WriteOnly));
    dataFile.write(signingInput.toLatin1());
    dataFile.close();

    QFile signatureFile(signaturePath);
    QVERIFY(signatureFile.open(QIODevice::WriteOnly));
    signatureFile.write(signature);
    signatureFile.close();

    QProcess verify;
    verify.start(QStringLiteral("openssl"),
                 {"dgst", "-sha256", "-verify", m_publicKeyPath, "-signature", signaturePath,
                  dataPath});
    QVERIFY(verify.waitForFinished(30000));
    QCOMPARE(verify.exitCode(), 0);
}

QTEST_GUILESS_MAIN(TestJwt)
#include "tst_jwt.moc"
