#include "core/Money.h"
#include "data/TransactionStore.h"
#include "data/TrendsModel.h"

#include <QTemporaryDir>
#include <QTest>

/*!
 * Month-on-month spending comparison.
 *
 * Two things matter here and neither is obvious from the numbers alone: the
 * comparison has to count exactly what the dashboard counts, or the same month
 * reads differently on two screens; and a change has to be honest about the
 * cases where there is no percentage to report.
 */
class TestTrends : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void groupsSpendingByMonthAndCategory();
    void countsOnlyWhatTheDashboardCounts();
    void ordersByTheMostRecentMonth();
    void reportsTheChangeAgainstThePreviousMonth();
    void reportsNoDirectionWithoutAnEarlierMonth();
    void reportsAnAmountWhenThereIsNoPercentage();
    void ignoresAChangeTooSmallToMatter();
    void keepsAMonthWithNoSpendingAsAColumn();

private:
    static Transaction make(const QString &id, const QDate &date, qint64 amountMinor,
                            const QString &category, const QString &currency = QStringLiteral("EUR"));
    static QList<QDate> monthsEnding(const QDate &last, int count);

    QTemporaryDir m_directory;
};

Transaction TestTrends::make(const QString &id, const QDate &date, qint64 amountMinor,
                             const QString &category, const QString &currency)
{
    Transaction transaction;
    transaction.id = id;
    transaction.source = QStringLiteral("csv");
    transaction.institution = QStringLiteral("Aktia");
    transaction.accountId = QStringLiteral("acc");
    transaction.bookingDate = date;
    transaction.description = QStringLiteral("Shop");
    transaction.amountMinor = amountMinor;
    transaction.currency = currency;
    transaction.category = category;
    transaction.status = QStringLiteral("BOOK");
    return transaction;
}

QList<QDate> TestTrends::monthsEnding(const QDate &last, int count)
{
    QList<QDate> months;
    for (int offset = count - 1; offset >= 0; --offset)
        months.append(last.addMonths(-offset));
    return months;
}

void TestTrends::init()
{
}

void TestTrends::groupsSpendingByMonthAndCategory()
{
    TransactionStore store;
    QVERIFY(store.open(m_directory.filePath(QStringLiteral("a.sqlite"))));
    QCOMPARE(store.upsert({
                 make("1", QDate(2026, 8, 3), -1000, "Groceries"),
                 make("2", QDate(2026, 8, 20), -500, "Groceries"),
                 make("3", QDate(2026, 9, 4), -2000, "Groceries"),
                 make("4", QDate(2026, 9, 5), -700, "Transport"),
             }),
             4);

    const auto spending = store.monthlySpend(QDate(2026, 8, 1), QDate(2026, 9, 30),
                                             QStringLiteral("EUR"));
    QCOMPARE(spending.size(), 3);

    // August groceries are summed rather than listed twice.
    const auto august = std::find_if(spending.begin(), spending.end(), [](const auto &entry) {
        return entry.month == QDate(2026, 8, 1) && entry.category == QStringLiteral("Groceries");
    });
    QVERIFY(august != spending.end());
    QCOMPARE(august->spentMinor, 1500);
}

void TestTrends::countsOnlyWhatTheDashboardCounts()
{
    TransactionStore store;
    QVERIFY(store.open(m_directory.filePath(QStringLiteral("b.sqlite"))));

    QCOMPARE(store.upsert({
                 make("keep", QDate(2026, 9, 4), -2000, "Groceries"),
                 make("skip", QDate(2026, 9, 6), -5000, "Electronics"),
                 // Income is not spending.
                 make("in", QDate(2026, 9, 7), 250000, "Income"),
                 // Moving money between your own accounts is not spending either.
                 make("xfer", QDate(2026, 9, 8), -20000, "Transfers"),
                 // Another currency cannot be added to this total.
                 make("gbp", QDate(2026, 9, 9), -3000, "Groceries", QStringLiteral("GBP")),
             }),
             5);
    // Set the way the user sets it: upsert deliberately never marks a row
    // excluded, because that judgement is the user's and a provider must not
    // be able to make or unmake it.
    QVERIFY(store.setExcluded(QStringLiteral("skip"), true));

    const auto spending = store.monthlySpend(QDate(2026, 9, 1), QDate(2026, 9, 30),
                                             QStringLiteral("EUR"));
    QCOMPARE(spending.size(), 1);
    QCOMPARE(spending.first().category, QStringLiteral("Groceries"));
    QCOMPARE(spending.first().spentMinor, 2000);
}

void TestTrends::ordersByTheMostRecentMonth()
{
    TrendsModel model;
    const QList<QDate> months = monthsEnding(QDate(2026, 9, 1), 2);
    // Travel dominates the older month; Groceries the newer one. The newer is
    // what the user is looking at, so it decides the order.
    model.setSpending({{QDate(2026, 8, 1), QStringLiteral("Travel"), 90000},
                       {QDate(2026, 8, 1), QStringLiteral("Groceries"), 1000},
                       {QDate(2026, 9, 1), QStringLiteral("Groceries"), 5000},
                       {QDate(2026, 9, 1), QStringLiteral("Travel"), 100}},
                      months, QStringLiteral("EUR"));

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.data(model.index(0, 0), TrendsModel::CategoryRole).toString(),
             QStringLiteral("Groceries"));
}

void TestTrends::reportsTheChangeAgainstThePreviousMonth()
{
    TrendsModel model;
    model.setSpending({{QDate(2026, 8, 1), QStringLiteral("Groceries"), 10000},
                       {QDate(2026, 9, 1), QStringLiteral("Groceries"), 12500}},
                      monthsEnding(QDate(2026, 9, 1), 2), QStringLiteral("EUR"));

    QCOMPARE(model.data(model.index(0, 0), TrendsModel::ChangeTextRole).toString(),
             QStringLiteral("+25%"));
    QCOMPARE(model.data(model.index(0, 0), TrendsModel::ChangeDirectionRole).toInt(), 1);
    QCOMPARE(model.totalChangeText(), QStringLiteral("+25%"));
    QCOMPARE(model.totalChangeDirection(), 1);
    QCOMPARE(model.latestTotalText(), Money::format(12500, QStringLiteral("EUR")));
}

void TestTrends::reportsNoDirectionWithoutAnEarlierMonth()
{
    TrendsModel model;
    // A single month cannot be up or down on anything; claiming a direction
    // would colour the very first month the user ever syncs.
    model.setSpending({{QDate(2026, 9, 1), QStringLiteral("Groceries"), 5000}},
                      monthsEnding(QDate(2026, 9, 1), 1), QStringLiteral("EUR"));

    QVERIFY(model.data(model.index(0, 0), TrendsModel::ChangeTextRole).toString().isEmpty());
    QCOMPARE(model.data(model.index(0, 0), TrendsModel::ChangeDirectionRole).toInt(), 0);
    QCOMPARE(model.totalChangeDirection(), 0);
}

void TestTrends::reportsAnAmountWhenThereIsNoPercentage()
{
    TrendsModel model;
    // Nothing last month, something this month: the percentage is infinite, so
    // the amount is the only honest thing to show.
    model.setSpending({{QDate(2026, 9, 1), QStringLiteral("Hobbies"), 4200}},
                      monthsEnding(QDate(2026, 9, 1), 2), QStringLiteral("EUR"));

    const QString change = model.data(model.index(0, 0), TrendsModel::ChangeTextRole).toString();
    QVERIFY2(!change.contains(QLatin1Char('%')), qPrintable(change));
    QVERIFY(change.startsWith(QLatin1Char('+')));
    QCOMPARE(model.data(model.index(0, 0), TrendsModel::ChangeDirectionRole).toInt(), 1);
}

void TestTrends::ignoresAChangeTooSmallToMatter()
{
    TrendsModel model;
    // Fifty cents on a hundred euros is not a trend worth colouring.
    model.setSpending({{QDate(2026, 8, 1), QStringLiteral("Groceries"), 10000},
                       {QDate(2026, 9, 1), QStringLiteral("Groceries"), 10050}},
                      monthsEnding(QDate(2026, 9, 1), 2), QStringLiteral("EUR"));

    QCOMPARE(model.data(model.index(0, 0), TrendsModel::ChangeDirectionRole).toInt(), 0);
}

void TestTrends::keepsAMonthWithNoSpendingAsAColumn()
{
    TrendsModel model;
    // August is empty. It must still occupy a column, or the run of months
    // closes up and a gap in spending reads as a continuous sequence.
    model.setSpending({{QDate(2026, 7, 1), QStringLiteral("Groceries"), 1000},
                       {QDate(2026, 9, 1), QStringLiteral("Groceries"), 3000}},
                      monthsEnding(QDate(2026, 9, 1), 3), QStringLiteral("EUR"));

    QCOMPARE(model.months().size(), 3);
    const QVariantList monthly =
        model.data(model.index(0, 0), TrendsModel::MonthlyRole).toList();
    QCOMPARE(monthly.size(), 3);
    QCOMPARE(monthly.at(1).toMap().value(QStringLiteral("amountMinor")).toLongLong(), 0);
    // And the empty month is what the latest is compared against.
    QCOMPARE(model.data(model.index(0, 0), TrendsModel::ChangeDirectionRole).toInt(), 1);
}

QTEST_GUILESS_MAIN(TestTrends)
#include "tst_trends.moc"
