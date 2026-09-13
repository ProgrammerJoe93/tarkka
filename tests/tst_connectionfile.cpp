#include "auth/ConnectionFile.h"

#include <QTest>

/*!
 * The file that carries bank connections between devices.
 *
 * Its job is to be applied to a *different* machine's settings, so the parser
 * has to refuse anything it does not recognise rather than apply the fields it
 * happens to find: a half-applied import would leave the receiving device with
 * an application id that does not match its private key, and nothing but
 * "Wrong signature" to explain it.
 */
class TestConnectionFile : public QObject
{
    Q_OBJECT

private slots:
    void roundTripsConnections();
    void neverCarriesThePrivateKey();
    void rejectsUnrelatedJson();
    void rejectsAFileWithNoApplicationId();
    void rejectsAFormatFromTheFuture();
    void ignoresBlankSessionIds();
};

void TestConnectionFile::roundTripsConnections()
{
    ConnectionFile::Contents original;
    original.applicationId = QStringLiteral("00000000-0000-0000-0000-000000000000");
    original.country = QStringLiteral("FI");
    original.sessionIds = {QStringLiteral("session-one"), QStringLiteral("session-two")};

    QString errorMessage;
    const ConnectionFile::Contents parsed =
        ConnectionFile::parse(ConnectionFile::serialise(original), &errorMessage);

    QVERIFY2(parsed.isValid(), qPrintable(errorMessage));
    QCOMPARE(parsed.applicationId, original.applicationId);
    QCOMPARE(parsed.country, original.country);
    QCOMPARE(parsed.sessionIds, original.sessionIds);
}

void TestConnectionFile::neverCarriesThePrivateKey()
{
    // The file is meant to be moved through cloud storage, so it must stay
    // useless on its own. Session ids only reach an account when paired with
    // the application's key.
    ConnectionFile::Contents contents;
    contents.applicationId = QStringLiteral("app-id");
    contents.sessionIds = {QStringLiteral("session-one")};

    const QByteArray json = ConnectionFile::serialise(contents);
    QVERIFY(!json.contains("PRIVATE KEY"));
    QVERIFY(!json.contains("privateKey"));
    QVERIFY(!json.contains("keyPath"));
}

void TestConnectionFile::rejectsUnrelatedJson()
{
    QString errorMessage;
    // Valid JSON, and an object, but nothing to do with this app. Without the
    // marker check the import would clear the application id.
    const ConnectionFile::Contents parsed =
        ConnectionFile::parse(R"({"applicationId": "not-ours"})", &errorMessage);

    QVERIFY(!parsed.isValid());
    QVERIFY(!errorMessage.isEmpty());
}

void TestConnectionFile::rejectsAFileWithNoApplicationId()
{
    QString errorMessage;
    const ConnectionFile::Contents parsed = ConnectionFile::parse(
        R"({"tarkka": "connection", "version": 1, "sessionIds": ["a"]})", &errorMessage);

    QVERIFY(!parsed.isValid());
    QVERIFY(!errorMessage.isEmpty());
}

void TestConnectionFile::rejectsAFormatFromTheFuture()
{
    QString errorMessage;
    const ConnectionFile::Contents parsed = ConnectionFile::parse(
        R"({"tarkka": "connection", "version": 99, "applicationId": "app-id"})", &errorMessage);

    QVERIFY(!parsed.isValid());
    QVERIFY(errorMessage.contains(QStringLiteral("99")));
}

void TestConnectionFile::ignoresBlankSessionIds()
{
    QString errorMessage;
    const ConnectionFile::Contents parsed = ConnectionFile::parse(
        R"({"tarkka": "connection", "version": 1, "applicationId": "app-id",
            "sessionIds": ["a", "", "  ", "b"]})",
        &errorMessage);

    QVERIFY2(parsed.isValid(), qPrintable(errorMessage));
    QCOMPARE(parsed.sessionIds, QStringList({QStringLiteral("a"), QStringLiteral("b")}));
}

QTEST_APPLESS_MAIN(TestConnectionFile)
#include "tst_connectionfile.moc"
