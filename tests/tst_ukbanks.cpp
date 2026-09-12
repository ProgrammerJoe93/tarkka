#include "core/Categoriser.h"
#include "providers/CsvProvider.h"

#include <QTest>

/*!
 * Real UK bank export layouts.
 *
 * These differ from the Nordic ones in two ways that broke the importer
 * outright: several banks split direction across a Debit/Credit column pair
 * instead of signing a single amount, and most name no currency at all, which
 * would have filed sterling into the euro totals.
 */
class TestUkBanks : public QObject
{
    Q_OBJECT

private slots:
    void lloydsSplitsDebitAndCredit();
    void starlingQualifiesTheAmountHeader();
    void natwestUsesValue();
    void barclaysUsesMemo();
    void monzoCarriesItsOwnCurrency();
    void defaultCurrencyAppliesWhenTheFileNamesNone();
    void readsUkDatesAsDayFirst();
    void categorisesUkMerchants_data();
    void categorisesUkMerchants();
};

void TestUkBanks::lloydsSplitsDebitAndCredit()
{
    const QByteArray csv =
        "Transaction Date,Transaction Type,Sort Code,Account Number,Transaction Description,"
        "Debit Amount,Credit Amount,Balance\n"
        "04/09/2026,DEB,'20-00-00,12345678,TESCO STORES 3345,42.15,,1200.00\n"
        "01/09/2026,BGC,'20-00-00,12345678,ACME LTD SALARY,,2500.00,3700.00\n";

    CsvProvider provider;
    QString errorMessage;
    const TransactionList rows = provider.parse(csv, QStringLiteral("Lloyds"),
                                                &errorMessage);

    QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));
    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows.at(0).bookingDate, QDate(2026, 9, 4));
    QCOMPARE(rows.at(0).amountMinor, -4215LL);
    QVERIFY(rows.at(0).isSpend());
    QCOMPARE(rows.at(1).amountMinor, 250000LL);
    QVERIFY(!rows.at(1).isSpend());
}

void TestUkBanks::starlingQualifiesTheAmountHeader()
{
    // The currency appears only in the column heading.
    const QByteArray csv =
        "Date,Counter Party,Reference,Type,Amount (GBP),Balance (GBP),Spending Category,Notes\n"
        "04/09/2026,Pret A Manger,Card,FASTER PAYMENT,-4.85,510.20,EATING_OUT,\n";

    CsvProvider provider;
    QString errorMessage;
    const TransactionList rows = provider.parse(csv, QStringLiteral("Starling"), &errorMessage);

    QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().amountMinor, -485LL);
    QCOMPARE(rows.first().currency, QStringLiteral("GBP"));
    QCOMPARE(rows.first().counterparty, QStringLiteral("Pret A Manger"));
}

void TestUkBanks::natwestUsesValue()
{
    const QByteArray csv =
        "Date,Type,Description,Value,Balance,Account Name,Account Number\n"
        "06/09/2026,POS,BOOTS 1234 LONDON,-12.99,880.10,MR J RUSSELL,12345678\n";

    CsvProvider provider;
    const TransactionList rows = provider.parse(csv, QStringLiteral("NatWest"),
                                                nullptr);

    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().amountMinor, -1299LL);
    QCOMPARE(rows.first().description, QStringLiteral("BOOTS 1234 LONDON"));
}

void TestUkBanks::barclaysUsesMemo()
{
    const QByteArray csv =
        "Number,Date,Account,Amount,Subcategory,Memo\n"
        ",07/09/2026,20-00-00 12345678,-31.40,,SAINSBURYS S/MKT\n";

    CsvProvider provider;
    const TransactionList rows = provider.parse(csv, QStringLiteral("Barclays"), nullptr);

    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().amountMinor, -3140LL);
    QCOMPARE(rows.first().description, QStringLiteral("SAINSBURYS S/MKT"));
}

void TestUkBanks::monzoCarriesItsOwnCurrency()
{
    const QByteArray csv =
        "Transaction ID,Date,Time,Type,Name,Category,Amount,Currency,Local amount,"
        "Local currency,Notes and #tags,Description\n"
        "tx_1,08/09/2026,12:01:00,Card payment,Greggs,Eating out,-3.60,GBP,-3.60,GBP,,Greggs\n";

    CsvProvider provider;
    // An explicit currency column must win over the caller's default.
    const TransactionList rows = provider.parse(csv, QStringLiteral("Monzo"), nullptr);

    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().currency, QStringLiteral("GBP"));
    QCOMPARE(rows.first().amountMinor, -360LL);
}

void TestUkBanks::defaultCurrencyAppliesWhenTheFileNamesNone()
{
    const QByteArray csv =
        "Transaction Date,Transaction Description,Debit Amount,Credit Amount\n"
        "04/09/2026,WAITROSE 123,18.40,\n";

    CsvProvider provider;
    provider.setDefaultCurrency(QStringLiteral("GBP"));
    const TransactionList rows = provider.parse(csv, QStringLiteral("Lloyds"), nullptr);

    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().currency, QStringLiteral("GBP"));

    // Without being told, the importer must not invent sterling either.
    CsvProvider untold;
    QCOMPARE(untold.parse(csv, QStringLiteral("Lloyds"), nullptr).first().currency,
             QStringLiteral("EUR"));
}

void TestUkBanks::readsUkDatesAsDayFirst()
{
    // 03/04 is 3 April in a UK export, not 4 March.
    const QByteArray csv = "Date,Description,Amount\n03/04/2026,SHOP,-1.00\n";

    CsvProvider provider;
    const TransactionList rows = provider.parse(csv, QStringLiteral("HSBC"), nullptr);

    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().bookingDate, QDate(2026, 4, 3));
}

void TestUkBanks::categorisesUkMerchants_data()
{
    QTest::addColumn<QString>("merchant");
    QTest::addColumn<QString>("category");

    QTest::newRow("waitrose") << "WAITROSE 123" << "Groceries";
    QTest::newRow("ocado") << "OCADO RETAIL LTD" << "Groceries";
    QTest::newRow("greggs") << "GREGGS PLC" << "Restaurants & Cafes";
    QTest::newRow("pret") << "PRET A MANGER" << "Restaurants & Cafes";
    QTest::newRow("tfl") << "TFL TRAVEL CHARGE" << "Transport";
    QTest::newRow("trainline") << "TRAINLINE.COM" << "Transport";
    QTest::newRow("boots") << "BOOTS 1234" << "Health & Pharmacy";
    QTest::newRow("argos") << "ARGOS LTD" << "Shopping";
    QTest::newRow("council tax") << "COUNCIL TAX LBTH" << "Housing & Utilities";
    QTest::newRow("octopus") << "OCTOPUS ENERGY" << "Housing & Utilities";
    QTest::newRow("puregym") << "PUREGYM LTD" << "Entertainment";
}

void TestUkBanks::categorisesUkMerchants()
{
    QFETCH(QString, merchant);
    QFETCH(QString, category);

    Transaction transaction;
    transaction.counterparty = merchant;
    transaction.amountMinor = -1000;

    Categoriser categoriser;
    QCOMPARE(categoriser.categorise(transaction), category);
}

QTEST_APPLESS_MAIN(TestUkBanks)
#include "tst_ukbanks.moc"
