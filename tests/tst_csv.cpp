#include "providers/CsvParser.h"
#include "providers/CsvProvider.h"

#include <QTest>

class TestCsv : public QObject
{
    Q_OBJECT

private slots:
    void detectsDelimiter_data();
    void detectsDelimiter();
    void parsesQuotedFields();
    void parsesEmbeddedNewline();
    void findsColumnsByAlias();
    void importsRevolutExport();
    void importsNordeaFinnishExport();
    void foldsRevolutFeeIntoAmount();
    void reportsAnUnrecognisedHeader();
    void assignsDistinctIdsToIdenticalRows();
    void decodesLatin1();
};

void TestCsv::detectsDelimiter_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QChar>("expected");

    QTest::newRow("comma") << "a,b,c\n1,2,3" << QChar(',');
    QTest::newRow("semicolon") << "a;b;c\n1;2;3" << QChar(';');
    QTest::newRow("tab") << "a\tb\tc\n1\t2\t3" << QChar('\t');
    QTest::newRow("commas inside quotes are ignored")
        << "\"a,b\";c;d\n1;2;3" << QChar(';');
}

void TestCsv::detectsDelimiter()
{
    QFETCH(QString, text);
    QFETCH(QChar, expected);
    QCOMPARE(Csv::detectDelimiter(text), expected);
}

void TestCsv::parsesQuotedFields()
{
    const Csv::Table table = Csv::parse(QStringLiteral(
        "Name,Amount\n\"Cafe \"\"Roasted\"\", Helsinki\",-4.20\n"));

    QCOMPARE(table.header, QStringList({"Name", "Amount"}));
    QCOMPARE(table.rows.size(), 1);
    QCOMPARE(table.rows.first().first(), QStringLiteral("Cafe \"Roasted\", Helsinki"));
    QCOMPARE(table.rows.first().last(), QStringLiteral("-4.20"));
}

void TestCsv::parsesEmbeddedNewline()
{
    const Csv::Table table = Csv::parse(QStringLiteral(
        "Name,Note\n\"Shop\",\"line one\nline two\"\n"));

    QCOMPARE(table.rows.size(), 1);
    QVERIFY(table.rows.first().last().contains(QLatin1Char('\n')));
}

void TestCsv::findsColumnsByAlias()
{
    const QStringList header = {QStringLiteral("﻿Kirjauspäivä"), QStringLiteral(" Määrä "),
                                QStringLiteral("Saaja/Maksaja")};

    QCOMPARE(Csv::columnIndex(header, {QStringLiteral("kirjauspäivä")}), 0);
    QCOMPARE(Csv::columnIndex(header, {QStringLiteral("Määrä")}), 1);
    QCOMPARE(Csv::columnIndex(header, {QStringLiteral("nope")}), -1);
}

void TestCsv::importsRevolutExport()
{
    const QByteArray data =
        "Type,Product,Started Date,Completed Date,Description,Amount,Fee,Currency,State,Balance\n"
        "CARD_PAYMENT,Current,2026-09-01 09:12:03,2026-09-02 10:00:00,K-Market Kamppi,-24.90,0.00,"
        "EUR,COMPLETED,975.10\n"
        "TOPUP,Current,2026-09-03 08:00:00,2026-09-03 08:00:01,Payment from Nordea,500.00,0.00,"
        "EUR,COMPLETED,1475.10\n";

    CsvProvider provider;
    QString errorMessage;
    const TransactionList transactions = provider.parse(data, QString(), &errorMessage);

    QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));
    QCOMPARE(transactions.size(), 2);

    QCOMPARE(transactions.at(0).institution, QStringLiteral("Revolut"));
    QCOMPARE(transactions.at(0).bookingDate, QDate(2026, 9, 2));
    QCOMPARE(transactions.at(0).amountMinor, -2490LL);
    QCOMPARE(transactions.at(0).currency, QStringLiteral("EUR"));
    QCOMPARE(transactions.at(0).description, QStringLiteral("K-Market Kamppi"));
    QVERIFY(transactions.at(0).isSpend());

    QCOMPARE(transactions.at(1).amountMinor, 50000LL);
    QVERIFY(!transactions.at(1).isSpend());
}

void TestCsv::importsNordeaFinnishExport()
{
    const QByteArray data = QString::fromUtf8(
        "Kirjauspäivä;Arvopäivä;Määrä;Saaja/Maksaja;Tilinumero;Viite;Viesti;Valuutta\n"
        "12.09.2026;12.09.2026;-45,80;PRISMA ITAKESKUS;FI12;;Ostos;EUR\n"
        "01.09.2026;01.09.2026;2500,00;TYONANTAJA OY;FI34;12345;Palkka;EUR\n").toUtf8();

    CsvProvider provider;
    QString errorMessage;
    const TransactionList transactions = provider.parse(data, QString(), &errorMessage);

    QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));
    QCOMPARE(transactions.size(), 2);

    QCOMPARE(transactions.at(0).institution, QStringLiteral("Nordea"));
    QCOMPARE(transactions.at(0).bookingDate, QDate(2026, 9, 12));
    QCOMPARE(transactions.at(0).amountMinor, -4580LL);
    QCOMPARE(transactions.at(0).counterparty, QStringLiteral("PRISMA ITAKESKUS"));

    QCOMPARE(transactions.at(1).amountMinor, 250000LL);
    QCOMPARE(transactions.at(1).reference, QStringLiteral("12345"));
}

void TestCsv::foldsRevolutFeeIntoAmount()
{
    const QByteArray data =
        "Type,Product,Started Date,Completed Date,Description,Amount,Fee,Currency,State,Balance\n"
        "EXCHANGE,Current,2026-09-04 12:00:00,2026-09-04 12:00:01,Exchange to USD,-100.00,1.25,"
        "EUR,COMPLETED,875.10\n";

    CsvProvider provider;
    const TransactionList transactions = provider.parse(data, QString(), nullptr);

    QCOMPARE(transactions.size(), 1);
    QCOMPARE(transactions.first().amountMinor, -10125LL);
}

void TestCsv::reportsAnUnrecognisedHeader()
{
    CsvProvider provider;
    QString errorMessage;
    const TransactionList transactions =
        provider.parse("Colour,Shape\nred,round\n", QString(), &errorMessage);

    QVERIFY(transactions.isEmpty());
    QVERIFY(errorMessage.contains(QStringLiteral("date")));
    QVERIFY(errorMessage.contains(QStringLiteral("Colour")));
}

void TestCsv::assignsDistinctIdsToIdenticalRows()
{
    const QByteArray data =
        "Type,Product,Started Date,Completed Date,Description,Amount,Fee,Currency,State,Balance\n"
        "CARD_PAYMENT,Current,2026-09-01 09:00:00,2026-09-01 09:00:00,Coffee,-3.50,0.00,EUR,"
        "COMPLETED,10.00\n"
        "CARD_PAYMENT,Current,2026-09-01 15:00:00,2026-09-01 15:00:00,Coffee,-3.50,0.00,EUR,"
        "COMPLETED,6.50\n";

    CsvProvider provider;
    const TransactionList transactions = provider.parse(data, QString(), nullptr);

    QCOMPARE(transactions.size(), 2);
    QVERIFY(transactions.at(0).id != transactions.at(1).id);
}

void TestCsv::decodesLatin1()
{
    // "Määrä" encoded as Latin-1 is not valid UTF-8 and must not be mangled.
    QByteArray data = QStringLiteral("Kirjauspäivä;Määrä\n01.09.2026;-1,00\n").toLatin1();
    const QString decoded = Csv::decode(data);
    QVERIFY(decoded.contains(QStringLiteral("Määrä")));

    CsvProvider provider;
    const TransactionList transactions = provider.parse(data, QStringLiteral("Nordea"), nullptr);
    QCOMPARE(transactions.size(), 1);
    QCOMPARE(transactions.first().amountMinor, -100LL);
}

QTEST_APPLESS_MAIN(TestCsv)
#include "tst_csv.moc"
