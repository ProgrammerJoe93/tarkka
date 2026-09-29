#include "core/Categoriser.h"
#include "providers/CsvProvider.h"
#include "providers/CsvParser.h"
#include "providers/Trading212Csv.h"
#include "providers/Trading212Provider.h"

#include <QSet>
#include <QTest>

class TestTrading212 : public QObject
{
    Q_OBJECT

private slots:
    void parsesTheCardOnlyExportHeader();
    void marksOnlyActionDerivedCategoriesAsAuthoritative();
    void recognisesTheExportHeader();
    void doesNotClaimAPlainBankExport();
    void mapsCardSpending();
    void treatsDepositsAndWithdrawalsAsTransfers();
    void dropsPortfolioActivity();
    void keepsInterestAndCashback();
    void usesTheReportedMerchantCategory();
    void routesThroughTheGenericImporter();
    void categorisesTopUpsAsTransfersNotInvesting();
    void importsARealisticExport();
    void buildsABasicAuthorizationHeader();

private:
    static Csv::Table tableOf(const QByteArray &csv);
};

namespace {

// Columns as Trading 212 writes them, with the card pair appended at the end.
const QByteArray kHeader =
    "Action,Time,ISIN,Ticker,Name,No. of shares,Price / share,Currency (Price / share),"
    "Exchange rate,Result,Total,Currency (Total),Withholding tax,Charge amount,Notes,ID,"
    "Currency conversion fee,Merchant name,Merchant category\n";

} // namespace

Csv::Table TestTrading212::tableOf(const QByteArray &csv)
{
    return Csv::parse(Csv::decode(csv));
}

void TestTrading212::recognisesTheExportHeader()
{
    QVERIFY(Trading212::matches(tableOf(kHeader).header));
    QCOMPARE(CsvProvider::detectInstitution(tableOf(kHeader).header),
             QStringLiteral("Trading 212"));
}

void TestTrading212::doesNotClaimAPlainBankExport()
{
    const QStringList revolut = {"Type", "Product", "Started Date", "Completed Date",
                                 "Description", "Amount", "Fee", "Currency", "State"};
    QVERIFY(!Trading212::matches(revolut));

    const QStringList nordea = {"Kirjauspäivä", "Määrä", "Saaja/Maksaja"};
    QVERIFY(!Trading212::matches(nordea));
}

void TestTrading212::mapsCardSpending()
{
    const QByteArray csv = kHeader +
        "Card debit,2026-09-04 12:34:56,,,,,,,,,-24.90,EUR,,,,abc-123,,K-Market Kamppi,Groceries\n";

    QString errorMessage;
    const TransactionList rows = Trading212::parse(tableOf(csv), &errorMessage);

    QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().institution, QStringLiteral("Trading 212"));
    QCOMPARE(rows.first().bookingDate, QDate(2026, 9, 4));
    QCOMPARE(rows.first().amountMinor, -2490LL);
    QCOMPARE(rows.first().currency, QStringLiteral("EUR"));
    QCOMPARE(rows.first().counterparty, QStringLiteral("K-Market Kamppi"));
    QCOMPARE(rows.first().reference, QStringLiteral("abc-123"));
    QVERIFY(rows.first().isSpend());
}

void TestTrading212::treatsDepositsAndWithdrawalsAsTransfers()
{
    // The other leg of a deposit is already recorded on the funding account, so
    // counting it as income here would inflate the month twice over.
    const QByteArray csv = kHeader +
        "Deposit,2026-09-01 08:00:00,,,,,,,,,500.00,EUR,,,,dep-1,,,\n"
        "Withdrawal,2026-09-20 08:00:00,,,,,,,,,-100.00,EUR,,,,wd-1,,,\n";

    const TransactionList rows = Trading212::parse(tableOf(csv), nullptr);

    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows.at(0).category, Categoriser::transfers());
    QCOMPARE(rows.at(0).amountMinor, 50000LL);
    QCOMPARE(rows.at(1).category, Categoriser::transfers());
    QCOMPARE(rows.at(1).amountMinor, -10000LL);
}

void TestTrading212::dropsPortfolioActivity()
{
    // Buying shares moves cash into holdings; it is not money leaving the user.
    const QByteArray csv = kHeader +
        "Market buy,2026-09-05 10:00:00,US0378331005,AAPL,Apple,2,100.00,USD,1.1,,-200.00,EUR,"
        ",,,ord-1,,,\n"
        "Market sell,2026-09-06 10:00:00,US0378331005,AAPL,Apple,1,110.00,USD,1.1,10.00,100.00,"
        "EUR,,,,ord-2,,,\n"
        "Card debit,2026-09-07 10:00:00,,,,,,,,,-9.90,EUR,,,,card-1,,Wolt,Restaurants\n";

    const TransactionList rows = Trading212::parse(tableOf(csv), nullptr);

    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().counterparty, QStringLiteral("Wolt"));
}

void TestTrading212::keepsInterestAndCashback()
{
    const QByteArray csv = kHeader +
        "Interest on cash,2026-09-30 00:00:00,,,,,,,,,3.21,EUR,,,,int-1,,,\n"
        "Spending cashback,2026-09-15 00:00:00,,,,,,,,,1.05,EUR,,,,cb-1,,,\n";

    const TransactionList rows = Trading212::parse(tableOf(csv), nullptr);

    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows.at(0).category, QStringLiteral("Income"));
    QCOMPARE(rows.at(0).amountMinor, 321LL);
    QCOMPARE(rows.at(1).category, QStringLiteral("Cashback"));
    QVERIFY(!rows.at(1).isSpend());
}

void TestTrading212::usesTheReportedMerchantCategory()
{
    // The card's own category beats a guess from the merchant name.
    const QByteArray csv = kHeader +
        "Card debit,2026-09-04 12:00:00,,,,,,,,,-12.00,EUR,,,,x-1,,Some Unknown Vendor,Transport\n";

    const TransactionList rows = Trading212::parse(tableOf(csv), nullptr);

    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().category, QStringLiteral("Transport"));
}

void TestTrading212::routesThroughTheGenericImporter()
{
    // A user dropping the file on the import button must get the same result.
    const QByteArray csv = kHeader +
        "Card debit,2026-09-04 12:00:00,,,,,,,,,-24.90,EUR,,,,abc-123,,K-Market,Groceries\n";

    CsvProvider provider;
    QString errorMessage;
    const TransactionList rows = provider.parse(csv, QString(), &errorMessage);

    QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().institution, QStringLiteral("Trading 212"));
    QCOMPARE(rows.first().amountMinor, -2490LL);
}

void TestTrading212::categorisesTopUpsAsTransfersNotInvesting()
{
    // Seen from the funding bank account, money sent to Trading 212 funds a
    // spending card. Calling it investing would count it as spending, and the
    // card purchases it pays for would then be counted a second time.
    Categoriser categoriser;
    Transaction transaction;
    transaction.counterparty = QStringLiteral("TRADING 212 MARKETS LTD");
    transaction.amountMinor = -30000;

    QCOMPARE(categoriser.categorise(transaction), Categoriser::transfers());
}

void TestTrading212::importsARealisticExport()
{
    // A month as it actually comes out of the export: a top-up, card spending,
    // one share purchase, a refund, cashback and interest.
    const QByteArray csv = kHeader +
        "Deposit,2026-09-02 08:00:00,,,,,,,,,600.00,EUR,,,,dep-1,,,\n"
        "Card debit,2026-09-03 12:11:00,,,,,,,,,-42.15,EUR,,,,c-1,,K-Market Kamppi,Groceries\n"
        "Card debit,2026-09-05 19:02:00,,,,,,,,,-18.40,EUR,,,,c-2,,Wolt,Restaurants\n"
        "Card debit,2026-09-06 08:30:00,,,,,,,,,-3.20,EUR,,,,c-3,,HSL,Transport\n"
        "Market buy,2026-09-06 10:00:00,US0378331005,AAPL,Apple,2,100.00,USD,1.1,,-200.00,EUR,"
        ",,,ord-1,,,\n"
        "Card debit,2026-09-08 17:45:00,,,,,,,,,-64.90,EUR,,,,c-4,,Verkkokauppa.com,Shopping\n"
        "Spending cashback,2026-09-09 00:00:00,,,,,,,,,1.28,EUR,,,,cb-1,,,\n"
        "Card credit,2026-09-10 11:00:00,,,,,,,,,12.00,EUR,,,,c-5,,Verkkokauppa.com,Shopping\n"
        "Interest on cash,2026-09-11 00:00:00,,,,,,,,,2.44,EUR,,,,int-1,,,\n";

    CsvProvider provider;
    QString errorMessage;
    const TransactionList rows = provider.parse(csv, QString(), &errorMessage);

    QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));
    // Nine lines in, the share purchase dropped.
    QCOMPARE(rows.size(), 8);

    qint64 spent = 0;
    qint64 received = 0;
    qint64 transferred = 0;
    for (const Transaction &row : rows) {
        if (row.category == Categoriser::transfers())
            transferred += qAbs(row.amountMinor);
        else if (row.amountMinor < 0)
            spent += -row.amountMinor;
        else
            received += row.amountMinor;
    }

    QCOMPARE(spent, 4215LL + 1840LL + 320LL + 6490LL);
    QCOMPARE(received, 128LL + 1200LL + 244LL);
    QCOMPARE(transferred, 60000LL);

    // Every id must be distinct or the store would collapse rows on import.
    QSet<QString> ids;
    for (const Transaction &row : rows)
        ids.insert(row.id);
    QCOMPARE(ids.size(), rows.size());
}

void TestTrading212::buildsABasicAuthorizationHeader()
{
    // The API takes the key as the Basic username and the secret as the
    // password. Sending the bare key, as this first did, is simply rejected.
    const QByteArray header =
        Trading212Provider::basicAuthHeader(QStringLiteral("my-key"),
                                            QStringLiteral("my-secret"));

    QVERIFY(header.startsWith("Basic "));
    const QByteArray decoded = QByteArray::fromBase64(header.mid(6));
    QCOMPARE(decoded, QByteArrayLiteral("my-key:my-secret"));

    // No stray newline: base64 wrapping would invalidate the credential.
    QVERIFY(!header.contains('\n'));
}

void TestTrading212::parsesTheCardOnlyExportHeader()
{
    /*
     * The card-spending export, which carries no portfolio columns at all and
     * names its timestamp "Time (UTC)". Matching column names exactly rejected
     * the whole file over that bracketed suffix, reporting a missing Time
     * column while the header plainly showed one.
     */
    const QByteArray header =
        "Action,Time (UTC),Notes,ID,Total,Currency (Total),Currency conversion fee,"
        "Currency (Currency conversion fee),Merchant name,Merchant category\n";

    QVERIFY(Trading212::matches(tableOf(header).header));

    const QByteArray csv = header +
        "Card debit,2026-09-04 12:34:56,,abc-123,-24.90,EUR,0.00,EUR,K-Market Kamppi,Groceries\n";

    QString errorMessage;
    const TransactionList rows = Trading212::parse(tableOf(csv), &errorMessage);

    QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().bookingDate, QDate(2026, 9, 4));
    QCOMPARE(rows.first().amountMinor, -2490LL);
    QCOMPARE(rows.first().currency, QStringLiteral("EUR"));
    QCOMPARE(rows.first().counterparty, QStringLiteral("K-Market Kamppi"));
    QCOMPARE(rows.first().reference, QStringLiteral("abc-123"));
}

void TestTrading212::marksOnlyActionDerivedCategoriesAsAuthoritative()
{
    const QByteArray header =
        "Action,Time (UTC),Notes,ID,Total,Currency (Total),Currency conversion fee,"
        "Currency (Currency conversion fee),Merchant name,Merchant category\n";

    /*
     * Two rows, two kinds of category. The deposit is a transfer because of its
     * action, which no merchant rule could work out -- that one is the
     * provider's to keep. The card purchase carries the broker's own taxonomy
     * ("RETAIL_STORES"), which is a starting point the user's rules must be
     * able to override; marking it authoritative put those rows beyond reach.
     */
    const QByteArray csv = header +
        "Deposit,2026-09-02 09:00:00,,dep-1,500.00,EUR,0.00,EUR,,\n"
        "Card debit,2026-09-04 12:34:56,,card-1,-24.90,EUR,0.00,EUR,Verkkokauppa,RETAIL_STORES\n";

    QString errorMessage;
    const TransactionList rows = Trading212::parse(tableOf(csv), &errorMessage);
    QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));
    QCOMPARE(rows.size(), 2);

    for (const Transaction &row : rows) {
        if (row.category == QStringLiteral("Transfers")) {
            QVERIFY2(row.categoryFromProvider, "an action-derived category is the provider's");
        } else {
            QCOMPARE(row.category, QStringLiteral("RETAIL_STORES"));
            QVERIFY2(!row.categoryFromProvider,
                     "a reported merchant category is only a starting point");
        }
    }
}

QTEST_APPLESS_MAIN(TestTrading212)
#include "tst_trading212.moc"
