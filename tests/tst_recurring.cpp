#include "core/Recurring.h"

#include <QTest>

/*!
 * Detection of payments that repeat monthly.
 *
 * The tests that matter here are the negative ones. A list of standing payments
 * is only useful if it can be trusted at a glance, and a detector that sweeps in
 * coincidences has to be read carefully instead -- which is worse than not
 * having it. So most of this is about what must *not* qualify.
 */
class TestRecurring : public QObject
{
    Q_OBJECT

private slots:
    void findsAMonthlySubscription();
    void seesThroughNoisyCardDescriptors();
    void ignoresAWeeklyShopAtOneMerchant();
    void ignoresVaryingAmountsAtTheSameMerchant();
    void ignoresARunWithAMissedMonth();
    void ignoresTooFewOccurrences();
    void ignoresTransfersAndExcludedRows();
    void reportsTheLatestAmountSoAPriceRiseShows();

private:
    static Transaction pay(const QDate &date, qint64 amountMinor, const QString &merchant,
                           const QString &category = QStringLiteral("Subscriptions"));
};

Transaction TestRecurring::pay(const QDate &date, qint64 amountMinor, const QString &merchant,
                               const QString &category)
{
    Transaction transaction;
    transaction.id = merchant + date.toString(Qt::ISODate);
    transaction.source = QStringLiteral("csv");
    transaction.institution = QStringLiteral("Aktia");
    transaction.bookingDate = date;
    transaction.counterparty = merchant;
    transaction.amountMinor = amountMinor;
    transaction.currency = QStringLiteral("EUR");
    transaction.category = category;
    transaction.status = QStringLiteral("BOOK");
    return transaction;
}

void TestRecurring::findsAMonthlySubscription()
{
    const auto found = Recurring::find({pay(QDate(2026, 6, 12), -1199, "Spotify"),
                                        pay(QDate(2026, 7, 12), -1199, "Spotify"),
                                        pay(QDate(2026, 8, 12), -1199, "Spotify")},
                                       QStringLiteral("EUR"));

    QCOMPARE(found.size(), 1);
    QCOMPARE(found.first().merchant, QStringLiteral("Spotify"));
    QCOMPARE(found.first().typicalMinor, 1199);
    QCOMPARE(found.first().occurrences, 3);
    QCOMPARE(found.first().lastSeen, QDate(2026, 8, 12));
}

void TestRecurring::seesThroughNoisyCardDescriptors()
{
    // The same subscription, described differently every month: a changing
    // reference number, a city, inconsistent case and spacing. Matching the raw
    // text would find nothing at all.
    const auto found = Recurring::find({pay(QDate(2026, 6, 3), -1499, "NETFLIX.COM 4021"),
                                        pay(QDate(2026, 7, 4), -1499, "Netflix.com  5138"),
                                        pay(QDate(2026, 8, 3), -1499, "netflix com 6207")},
                                       QStringLiteral("EUR"));

    QCOMPARE(found.size(), 1);
    QCOMPARE(found.first().occurrences, 3);
}

void TestRecurring::ignoresAWeeklyShopAtOneMerchant()
{
    // Visited constantly, which is the most likely false positive of all.
    TransactionList weekly;
    for (int week = 0; week < 12; ++week)
        weekly.append(pay(QDate(2026, 6, 1).addDays(week * 7), -4200, "K-Market Kamppi"));

    QVERIFY(Recurring::find(weekly, QStringLiteral("EUR")).isEmpty());
}

void TestRecurring::ignoresVaryingAmountsAtTheSameMerchant()
{
    // Monthly rhythm, but the amount is different every time: a shop visited
    // once a month is not a standing payment.
    const auto found = Recurring::find({pay(QDate(2026, 6, 10), -2000, "Verkkokauppa"),
                                        pay(QDate(2026, 7, 10), -18000, "Verkkokauppa"),
                                        pay(QDate(2026, 8, 10), -4500, "Verkkokauppa")},
                                       QStringLiteral("EUR"));

    QVERIFY(found.isEmpty());
}

void TestRecurring::ignoresARunWithAMissedMonth()
{
    // A cancelled and later resumed subscription is not currently a commitment,
    // and a gap is how that looks.
    const auto found = Recurring::find({pay(QDate(2026, 5, 12), -1199, "Spotify"),
                                        pay(QDate(2026, 6, 12), -1199, "Spotify"),
                                        pay(QDate(2026, 9, 12), -1199, "Spotify")},
                                       QStringLiteral("EUR"));

    QVERIFY(found.isEmpty());
}

void TestRecurring::ignoresTooFewOccurrences()
{
    // Twice is a coincidence; the third time is a pattern.
    const auto found = Recurring::find({pay(QDate(2026, 7, 12), -1199, "Spotify"),
                                        pay(QDate(2026, 8, 12), -1199, "Spotify")},
                                       QStringLiteral("EUR"));

    QVERIFY(found.isEmpty());
}

void TestRecurring::ignoresTransfersAndExcludedRows()
{
    // A standing transfer to savings is regular, but it is not a payment to
    // anyone and the totals already leave it out.
    TransactionList rows = {pay(QDate(2026, 6, 1), -20000, "Own savings", "Transfers"),
                            pay(QDate(2026, 7, 1), -20000, "Own savings", "Transfers"),
                            pay(QDate(2026, 8, 1), -20000, "Own savings", "Transfers")};
    QVERIFY(Recurring::find(rows, QStringLiteral("EUR")).isEmpty());

    TransactionList setAside = {pay(QDate(2026, 6, 5), -999, "Gym"),
                                pay(QDate(2026, 7, 5), -999, "Gym"),
                                pay(QDate(2026, 8, 5), -999, "Gym")};
    for (Transaction &transaction : setAside)
        transaction.excluded = true;
    QVERIFY(Recurring::find(setAside, QStringLiteral("EUR")).isEmpty());
}

void TestRecurring::reportsTheLatestAmountSoAPriceRiseShows()
{
    // The point of the latest figure: a subscription repriced mid-run should be
    // visible as such rather than averaged away.
    const auto found = Recurring::find({pay(QDate(2026, 6, 12), -1199, "Spotify"),
                                        pay(QDate(2026, 7, 12), -1199, "Spotify"),
                                        pay(QDate(2026, 8, 12), -1399, "Spotify")},
                                       QStringLiteral("EUR"));

    QCOMPARE(found.size(), 1);
    QCOMPARE(found.first().typicalMinor, 1199);
    QCOMPARE(found.first().latestMinor, 1399);
}

QTEST_APPLESS_MAIN(TestRecurring)
#include "tst_recurring.moc"
