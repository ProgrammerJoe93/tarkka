#include "providers/EnableBankingProvider.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "providers/Provider.h"

#include <QTest>

class TestEnableBanking : public QObject
{
    Q_OBJECT

private slots:
    void clampsTheSyncWindowToToday();
    void narrowsToWhatABankWillServe();
    void doesNotNarrowAWindowAlreadyInsideTheCap();
    void doesNotNarrowAWindowEntirelyTooOld();
    void leavesAPastRangeAlone();
    void widensTheStartForLateBookings();
    void identifiesAPaymentAccountByIban();
    void identifiesACreditCardByPan();
    void namesAnUnnamedCardAccount();
    void handlesABareUidString();
    void mapsACardPurchase();
    void trustsTheIndicatorOverTheAmountSign();
    void fallsBackToTheAmountSignWhenNoIndicator();
    void picksTheCounterpartyForTheDirection();
    void skipsRowsWithNoUsableDate();

private:
    static QJsonArray arrayOf(const char *json);
    static AccountModel::Account cardAccount();
};

QJsonArray TestEnableBanking::arrayOf(const char *json)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(QByteArray(json), &error);
    Q_ASSERT(error.error == QJsonParseError::NoError);
    return document.array();
}

AccountModel::Account TestEnableBanking::cardAccount()
{
    AccountModel::Account account;
    account.uid = QStringLiteral("uid-card");
    account.institution = QStringLiteral("Aktia");
    account.name = QStringLiteral("Finnair Visa");
    account.currency = QStringLiteral("EUR");
    account.cashAccountType = QStringLiteral("CARD");
    return account;
}

void TestEnableBanking::identifiesAPaymentAccountByIban()
{
    const QJsonValue value = QJsonDocument::fromJson(R"({
        "uid": "uid-1",
        "name": "Käyttötili",
        "currency": "EUR",
        "cash_account_type": "CACC",
        "account_id": {"iban": "FI2112345600000785"}
    })").object();

    const AccountModel::Account account = EnableBankingProvider::parseAccount(
        value, QStringLiteral("s1"), QStringLiteral("Aktia"));

    QCOMPARE(account.identifier, QStringLiteral("FI2112345600000785"));
    QVERIFY(!account.isCard());
}

void TestEnableBanking::identifiesACreditCardByPan()
{
    // A card account carries no IBAN; the bank reports it under account_id.other.
    const QJsonValue value = QJsonDocument::fromJson(R"({
        "uid": "uid-card",
        "name": "Finnair Visa",
        "currency": "EUR",
        "cash_account_type": "CARD",
        "account_id": {"other": {"identification": "492500******1234",
                                 "scheme_name": "CPAN"}}
    })").object();

    const AccountModel::Account account = EnableBankingProvider::parseAccount(
        value, QStringLiteral("s1"), QStringLiteral("Aktia"));

    QCOMPARE(account.identifier, QStringLiteral("492500******1234"));
    QCOMPARE(account.name, QStringLiteral("Finnair Visa"));
    QVERIFY(account.isCard());
}

void TestEnableBanking::namesAnUnnamedCardAccount()
{
    const QJsonValue value = QJsonDocument::fromJson(R"({
        "uid": "uid-card",
        "cash_account_type": "CARD",
        "account_id": {"other": {"identification": "492500******1234"}}
    })").object();

    const AccountModel::Account account = EnableBankingProvider::parseAccount(
        value, QStringLiteral("s1"), QStringLiteral("Aktia"));

    QVERIFY(!account.name.isEmpty());
    QVERIFY(account.name != account.institution);
}

void TestEnableBanking::handlesABareUidString()
{
    const AccountModel::Account account = EnableBankingProvider::parseAccount(
        QJsonValue(QStringLiteral("uid-only")), QStringLiteral("s1"),
        QStringLiteral("Aktia"));

    QCOMPARE(account.uid, QStringLiteral("uid-only"));
    QCOMPARE(account.sessionId, QStringLiteral("s1"));
}

void TestEnableBanking::mapsACardPurchase()
{
    EnableBankingProvider provider;
    const TransactionList transactions = provider.parseTransactions(arrayOf(R"([{
        "entry_reference": "abc123",
        "booking_date": "2026-09-04",
        "value_date": "2026-09-05",
        "transaction_amount": {"currency": "EUR", "amount": "24.90"},
        "credit_debit_indicator": "DBIT",
        "status": "BOOK",
        "creditor": {"name": "FINNAIR OYJ"},
        "merchant_category_code": "3007",
        "remittance_information": ["Lento HEL-LHR"]
    }])"), cardAccount());

    QCOMPARE(transactions.size(), 1);
    const Transaction transaction = transactions.first();
    QCOMPARE(transaction.amountMinor, -2490LL);
    QVERIFY(transaction.isSpend());
    QCOMPARE(transaction.counterparty, QStringLiteral("FINNAIR OYJ"));
    QCOMPARE(transaction.description, QStringLiteral("Lento HEL-LHR"));
    QCOMPARE(transaction.institution, QStringLiteral("Aktia"));
    QCOMPARE(transaction.accountName, QStringLiteral("Finnair Visa"));
    QVERIFY(!transaction.isPending());
}

void TestEnableBanking::trustsTheIndicatorOverTheAmountSign()
{
    EnableBankingProvider provider;

    // The Berlin Group schema reports an unsigned amount alongside the
    // indicator, so the indicator decides the direction.
    const TransactionList credited = provider.parseTransactions(arrayOf(R"([{
        "booking_date": "2026-09-04",
        "transaction_amount": {"currency": "EUR", "amount": "150.00"},
        "credit_debit_indicator": "CRDT",
        "debtor": {"name": "Maksu kortille"}
    }])"), cardAccount());
    QCOMPARE(credited.size(), 1);
    QCOMPARE(credited.first().amountMinor, 15000LL);

    const TransactionList debited = provider.parseTransactions(arrayOf(R"([{
        "booking_date": "2026-09-04",
        "transaction_amount": {"currency": "EUR", "amount": "-150.00"},
        "credit_debit_indicator": "DBIT",
        "creditor": {"name": "Shop"}
    }])"), cardAccount());
    QCOMPARE(debited.size(), 1);
    QCOMPARE(debited.first().amountMinor, -15000LL);
}

void TestEnableBanking::fallsBackToTheAmountSignWhenNoIndicator()
{
    EnableBankingProvider provider;

    // credit_debit_indicator is optional. Treating its absence as "debit" used
    // to turn a repayment into another purchase.
    const TransactionList rows = provider.parseTransactions(arrayOf(R"([
        {"booking_date": "2026-09-04",
         "transaction_amount": {"currency": "EUR", "amount": "150.00"},
         "debtor": {"name": "Maksu kortille"}},
        {"booking_date": "2026-09-05",
         "transaction_amount": {"currency": "EUR", "amount": "-24.90"},
         "creditor": {"name": "FINNAIR OYJ"}}
    ])"), cardAccount());

    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows.at(0).amountMinor, 15000LL);
    QVERIFY(!rows.at(0).isSpend());
    QCOMPARE(rows.at(1).amountMinor, -2490LL);
    QVERIFY(rows.at(1).isSpend());
}

void TestEnableBanking::picksTheCounterpartyForTheDirection()
{
    EnableBankingProvider provider;
    const TransactionList rows = provider.parseTransactions(arrayOf(R"([
        {"booking_date": "2026-09-04",
         "transaction_amount": {"currency": "EUR", "amount": "10.00"},
         "credit_debit_indicator": "DBIT",
         "creditor": {"name": "Merchant"}, "debtor": {"name": "Me"}},
        {"booking_date": "2026-09-04",
         "transaction_amount": {"currency": "EUR", "amount": "10.00"},
         "credit_debit_indicator": "CRDT",
         "creditor": {"name": "Me"}, "debtor": {"name": "Payer"}}
    ])"), cardAccount());

    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows.at(0).counterparty, QStringLiteral("Merchant"));
    QCOMPARE(rows.at(1).counterparty, QStringLiteral("Payer"));
}

void TestEnableBanking::skipsRowsWithNoUsableDate()
{
    EnableBankingProvider provider;
    const TransactionList rows = provider.parseTransactions(arrayOf(R"([
        {"transaction_amount": {"currency": "EUR", "amount": "10.00"},
         "credit_debit_indicator": "DBIT"},
        {"value_date": "2026-09-06",
         "transaction_amount": {"currency": "EUR", "amount": "10.00"},
         "credit_debit_indicator": "DBIT"}
    ])"), cardAccount());

    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().bookingDate, QDate(2026, 9, 6));
}

void TestEnableBanking::clampsTheSyncWindowToToday()
{
    // The default range is the whole current month, so for all but the last
    // day of it the naive end date is in the future. S-Pankki rejects that
    // with HTTP 422 "Wrong transaction period requested" where other banks
    // quietly ignore it, which is why this is clamped rather than left to the
    // bank to tolerate.
    const QDate today(2026, 9, 12);
    const SyncWindow window =
        SyncWindow::forRange(QDate(2026, 9, 1), QDate(2026, 9, 30), today);

    QCOMPARE(window.to, today);
    QVERIFY(window.to <= today);
}

void TestEnableBanking::narrowsToWhatABankWillServe()
{
    const QDate today(2026, 9, 13);
    /*
     * June, viewed in September: the window starts before the ninety days a
     * bank will serve and ends after, so the bank refuses all of it rather
     * than returning the part it has. Retrying from the cap keeps the days it
     * was always willing to give.
     */
    const SyncWindow june = SyncWindow::forRange(QDate(2026, 6, 1), QDate(2026, 6, 30), today);
    QVERIFY(june.from < QDate(2026, 6, 15));
    QVERIFY(june.to > QDate(2026, 6, 15));

    const SyncWindow retry = SyncWindow::narrowed(june, today);
    QVERIFY(retry.isValid());
    QCOMPARE(retry.from, QDate(2026, 6, 15));
    QCOMPARE(retry.to, june.to);
}

void TestEnableBanking::doesNotNarrowAWindowAlreadyInsideTheCap()
{
    const QDate today(2026, 9, 13);
    // The current month is well inside ninety days, so a refusal was about
    // something else and asking again the same way would just fail twice.
    const SyncWindow september =
        SyncWindow::forRange(QDate(2026, 9, 1), QDate(2026, 9, 30), today);

    QVERIFY(!SyncWindow::narrowed(september, today).isValid());
}

void TestEnableBanking::doesNotNarrowAWindowEntirelyTooOld()
{
    const QDate today(2026, 9, 13);
    /*
     * May, viewed in September, is already wholly outside ninety days -- there
     * is no narrower window to ask for, because every day of it predates what
     * the bank will serve. The user gets told that rather than watching a
     * retry fail the same way.
     */
    const SyncWindow may = SyncWindow::forRange(QDate(2026, 5, 1), QDate(2026, 5, 31), today);
    QVERIFY(!SyncWindow::narrowed(may, today).isValid());

    // And the same for something far older still.
    const SyncWindow lastYear = SyncWindow::forRange(QDate(2025, 3, 1), QDate(2025, 3, 31), today);
    QVERIFY(!SyncWindow::narrowed(lastYear, today).isValid());
}

void TestEnableBanking::leavesAPastRangeAlone()
{
    // Browsing an earlier month must still fetch that month, not stop at today.
    const SyncWindow window =
        SyncWindow::forRange(QDate(2026, 3, 1), QDate(2026, 3, 31), QDate(2026, 9, 12));

    QCOMPARE(window.to, QDate(2026, 4, 1));
}

void TestEnableBanking::widensTheStartForLateBookings()
{
    const SyncWindow window =
        SyncWindow::forRange(QDate(2026, 9, 1), QDate(2026, 9, 30), QDate(2026, 9, 12));

    QCOMPARE(window.from, QDate(2026, 8, 25));
}

QTEST_GUILESS_MAIN(TestEnableBanking)
#include "tst_enablebanking.moc"
