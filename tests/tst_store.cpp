#include "core/Money.h"
#include "data/CategoryTotalsModel.h"
#include "data/SummaryModel.h"
#include "data/TransactionStore.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

class TestStore : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void insertsAndReadsBack();
    void deduplicatesOnResync();
    void keepsUserCategoryOnResync();
    void updatesPendingRowToBooked();
    void filtersByDateInstitutionAndCategory();
    void searchesText();
    void summarisesByCategory();
    void buildsDailySeriesWithGaps();
    void leavesInternalTransfersOutOfTheTotals();
    void addsTheExcludedColumnToAnOlderDatabase();
    void keepsAnExcludedRowExcludedOnResync();
    void leavesExcludedRowsOutOfTheTotals();
    void groupsIncomingMoneyByCategory();
    void appliesTheSameExclusionsToIncome();
    void countsExcludedTransfersOnlyOnce();
    void neverSumsAcrossCurrencies();
    void filtersByCurrency();

private:
    Transaction make(const QString &id, const QDate &date, qint64 amount,
                     const QString &institution, const QString &category,
                     const QString &counterparty = QStringLiteral("Shop"),
                     const QString &currency = QStringLiteral("EUR")) const;

    QTemporaryDir m_directory;
};

Transaction TestStore::make(const QString &id, const QDate &date, qint64 amount,
                            const QString &institution, const QString &category,
                            const QString &counterparty, const QString &currency) const
{
    Transaction transaction;
    transaction.id = id;
    transaction.source = QStringLiteral("test");
    transaction.institution = institution;
    transaction.accountId = institution + QStringLiteral("/main");
    transaction.accountName = QStringLiteral("Current");
    transaction.bookingDate = date;
    transaction.valueDate = date;
    transaction.counterparty = counterparty;
    transaction.description = counterparty;
    transaction.amountMinor = amount;
    transaction.currency = currency;
    transaction.category = category;
    transaction.status = QStringLiteral("BOOK");
    return transaction;
}

void TestStore::init()
{
    QVERIFY(m_directory.isValid());
}

void TestStore::insertsAndReadsBack()
{
    TransactionStore store;
    QString errorMessage;
    QVERIFY2(store.open(m_directory.filePath(QStringLiteral("a.sqlite")), &errorMessage),
             qPrintable(errorMessage));

    QCOMPARE(store.upsert({make("1", QDate(2026, 9, 1), -1000, "Nordea", "Groceries")}), 1);

    const TransactionList rows = store.query({});
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().amountMinor, -1000LL);
    QCOMPARE(rows.first().institution, QStringLiteral("Nordea"));
    QCOMPARE(rows.first().currency, QStringLiteral("EUR"));
}

void TestStore::deduplicatesOnResync()
{
    TransactionStore store;
    QVERIFY(store.open(m_directory.filePath(QStringLiteral("b.sqlite"))));

    const Transaction row = make("dup", QDate(2026, 9, 1), -1000, "Revolut", "Groceries");
    QCOMPARE(store.upsert({row}), 1);
    QCOMPARE(store.upsert({row}), 0);
    QCOMPARE(store.query({}).size(), 1);
}

void TestStore::keepsUserCategoryOnResync()
{
    TransactionStore store;
    QVERIFY(store.open(m_directory.filePath(QStringLiteral("c.sqlite"))));

    Transaction row = make("keep", QDate(2026, 9, 1), -1000, "Nordea", "Uncategorised");
    QCOMPARE(store.upsert({row}), 1);
    QVERIFY(store.setCategory(QStringLiteral("keep"), QStringLiteral("Hobbies")));

    // A re-sync brings the auto-guess back; the hand-set value must survive.
    row.category = QStringLiteral("Uncategorised");
    QCOMPARE(store.upsert({row}), 0);
    QCOMPARE(store.query({}).first().category, QStringLiteral("Hobbies"));
}

void TestStore::addsTheExcludedColumnToAnOlderDatabase()
{
    const QString path = m_directory.filePath(QStringLiteral("legacy.sqlite"));

    // A database exactly as a build before the exclusion flag left it. CREATE
    // TABLE IF NOT EXISTS will not touch this, so if the ALTER is missing the
    // store opens and then fails on every read.
    {
        QSqlDatabase legacy = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                        QStringLiteral("legacy"));
        legacy.setDatabaseName(path);
        QVERIFY(legacy.open());
        QSqlQuery query(legacy);
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE transactions ("
            "  id TEXT PRIMARY KEY, source TEXT NOT NULL, institution TEXT NOT NULL,"
            "  account_id TEXT NOT NULL, account_name TEXT, booking_date TEXT NOT NULL,"
            "  value_date TEXT, description TEXT, counterparty TEXT, reference TEXT,"
            "  amount_minor INTEGER NOT NULL, currency TEXT NOT NULL, category TEXT,"
            "  category_is_user_set INTEGER NOT NULL DEFAULT 0, status TEXT)")));
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO transactions (id, source, institution, account_id, booking_date,"
            "  amount_minor, currency, category)"
            " VALUES ('old', 'csv', 'Aktia', 'acc', '2026-09-01', -1000, 'EUR', 'Groceries')")));
    }
    QSqlDatabase::removeDatabase(QStringLiteral("legacy"));

    TransactionStore store;
    QVERIFY(store.open(path));

    // The existing row survives, counted, and can then be excluded.
    const TransactionList rows = store.query({});
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().id, QStringLiteral("old"));
    QVERIFY(!rows.first().excluded);

    QVERIFY(store.setExcluded(QStringLiteral("old"), true));
    QVERIFY(store.query({}).first().excluded);
}

void TestStore::keepsAnExcludedRowExcludedOnResync()
{
    TransactionStore store;
    QVERIFY(store.open(m_directory.filePath(QStringLiteral("x.sqlite"))));

    Transaction row = make("skip", QDate(2026, 9, 1), -1000, "Aktia", "Groceries");
    QCOMPARE(store.upsert({row}), 1);
    QVERIFY(!store.query({}).first().excluded);

    QVERIFY(store.setExcluded(QStringLiteral("skip"), true));
    QVERIFY(store.query({}).first().excluded);

    // The bank amends the row. The judgement to leave it out is the user's and
    // has to outlast whatever the provider sends next.
    row.amountMinor = -1200;
    QCOMPARE(store.upsert({row}), 0);
    const Transaction stored = store.query({}).first();
    QVERIFY(stored.excluded);
    QCOMPARE(stored.amountMinor, -1200);

    QVERIFY(store.setExcluded(QStringLiteral("skip"), false));
    QVERIFY(!store.query({}).first().excluded);
}

void TestStore::updatesPendingRowToBooked()
{
    TransactionStore store;
    QVERIFY(store.open(m_directory.filePath(QStringLiteral("d.sqlite"))));

    Transaction row = make("pend", QDate(2026, 9, 1), -1000, "Revolut", "Groceries");
    row.status = QStringLiteral("PDNG");
    QCOMPARE(store.upsert({row}), 1);
    QVERIFY(store.query({}).first().isPending());

    row.status = QStringLiteral("BOOK");
    row.amountMinor = -1050;
    QCOMPARE(store.upsert({row}), 0);

    const Transaction stored = store.query({}).first();
    QVERIFY(!stored.isPending());
    QCOMPARE(stored.amountMinor, -1050LL);
}

void TestStore::filtersByDateInstitutionAndCategory()
{
    TransactionStore store;
    QVERIFY(store.open(m_directory.filePath(QStringLiteral("e.sqlite"))));

    store.upsert({
        make("1", QDate(2026, 8, 15), -1000, "Nordea", "Groceries"),
        make("2", QDate(2026, 9, 5), -2000, "Revolut", "Transport"),
        make("3", QDate(2026, 9, 20), 5000, "Nordea", "Income"),
    });

    TransactionStore::Filter september;
    september.from = QDate(2026, 9, 1);
    september.to = QDate(2026, 9, 30);
    QCOMPARE(store.query(september).size(), 2);

    september.institution = QStringLiteral("Revolut");
    QCOMPARE(store.query(september).size(), 1);

    TransactionStore::Filter byCategory;
    byCategory.category = QStringLiteral("Groceries");
    QCOMPARE(store.query(byCategory).size(), 1);

    TransactionStore::Filter spendOnly;
    spendOnly.spendOnly = true;
    QCOMPARE(store.query(spendOnly).size(), 2);

    QCOMPARE(store.institutions(), QStringList({"Nordea", "Revolut"}));
    QCOMPARE(store.earliestDate(), QDate(2026, 8, 15));
}

void TestStore::searchesText()
{
    TransactionStore store;
    QVERIFY(store.open(m_directory.filePath(QStringLiteral("f.sqlite"))));

    store.upsert({
        make("1", QDate(2026, 9, 1), -1000, "Nordea", "Groceries", "K-Market Kamppi"),
        make("2", QDate(2026, 9, 2), -2000, "Nordea", "Transport", "HSL"),
    });

    TransactionStore::Filter filter;
    filter.searchText = QStringLiteral("kamppi");
    QCOMPARE(store.query(filter).size(), 1);
}

void TestStore::summarisesByCategory()
{
    SummaryModel summary;
    summary.setTransactions({
        make("1", QDate(2026, 9, 1), -1000, "Nordea", "Groceries"),
        make("2", QDate(2026, 9, 2), -3000, "Nordea", "Transport"),
        make("3", QDate(2026, 9, 3), -2000, "Nordea", "Groceries"),
        make("4", QDate(2026, 9, 4), 10000, "Nordea", "Income"),
    });

    QCOMPARE(summary.rowCount(), 2);
    // Groceries and Transport are tied at 30.00, so the name breaks the tie and
    // the order is stable across runs.
    QCOMPARE(summary.largestCategory(), QStringLiteral("Groceries"));
    QCOMPARE(summary.data(summary.index(1, 0), SummaryModel::CategoryRole).toString(),
             QStringLiteral("Transport"));
    QCOMPARE(summary.data(summary.index(0, 0), SummaryModel::AmountMinorRole).toLongLong(),
             3000LL);
    QCOMPARE(summary.data(summary.index(0, 0), SummaryModel::TransactionCountRole).toInt(), 2);
    QCOMPARE(summary.data(summary.index(0, 0), SummaryModel::ShareRole).toDouble(), 0.5);
}

void TestStore::buildsDailySeriesWithGaps()
{
    SummaryModel summary;
    summary.setTransactions({make("1", QDate(2026, 9, 1), -1000, "Nordea", "Groceries"),
                             make("2", QDate(2026, 9, 3), -2000, "Nordea", "Groceries")},
                            QDate(2026, 9, 1),
                            QDate(2026, 9, 4));

    const QVariantList series = summary.dailySpend();
    QCOMPARE(series.size(), 4);
    QCOMPARE(series.at(0).toMap().value(QStringLiteral("amountMinor")).toLongLong(), 1000LL);
    QCOMPARE(series.at(1).toMap().value(QStringLiteral("amountMinor")).toLongLong(), 0LL);
    QCOMPARE(series.at(2).toMap().value(QStringLiteral("share")).toDouble(), 1.0);
    QCOMPARE(series.at(3).toMap().value(QStringLiteral("amountMinor")).toLongLong(), 0LL);
}

void TestStore::leavesInternalTransfersOutOfTheTotals()
{
    SummaryModel summary;
    // Paying a credit card bill: one leg on each account. Counting either would
    // inflate the month's spending or income by the size of the repayment.
    summary.setTransactions({
        make("1", QDate(2026, 9, 1), -20000, "Aktia", "Transfers", "Maksu kortille"),
        make("2", QDate(2026, 9, 1), 20000, "Aktia", "Transfers", "Maksu kortille"),
        make("3", QDate(2026, 9, 2), -2490, "Aktia", "Transport", "FINNAIR OYJ"),
        make("4", QDate(2026, 9, 3), 250000, "Nordea", "Income", "Employer"),
    });

    QCOMPARE(summary.totalSpentText(), Money::format(2490, "EUR"));
    QCOMPARE(summary.totalReceivedText(), Money::format(250000, "EUR"));
    QCOMPARE(summary.netText(), Money::format(250000 - 2490, "EUR"));
    QVERIFY(summary.hasTransfers());
    QCOMPARE(summary.transfersText(), Money::format(20000, "EUR"));

    // The transfer must not appear as a spending category either.
    QCOMPARE(summary.rowCount(), 1);
    QCOMPARE(summary.data(summary.index(0, 0), SummaryModel::CategoryRole).toString(),
             QStringLiteral("Transport"));
}

void TestStore::leavesExcludedRowsOutOfTheTotals()
{
    SummaryModel summary;
    Transaction reimbursed = make("2", QDate(2026, 9, 2), -8000, "Aktia", "Groceries");
    reimbursed.excluded = true;

    summary.setTransactions({
        make("1", QDate(2026, 9, 1), -2490, "Aktia", "Transport"),
        reimbursed,
        make("3", QDate(2026, 9, 3), 250000, "Nordea", "Income"),
    });

    QCOMPARE(summary.totalSpentText(), Money::format(2490, "EUR"));
    QCOMPARE(summary.netText(), Money::format(250000 - 2490, "EUR"));
    QVERIFY(summary.hasExcluded());
    QCOMPARE(summary.excludedText(), Money::format(8000, "EUR"));

    // The category buckets have to agree with the totals, or the dashboard
    // would show a slice of spending the headline figure does not contain.
    QCOMPARE(summary.rowCount(), 1);
    QCOMPARE(summary.data(summary.index(0, 0), SummaryModel::CategoryRole).toString(),
             QStringLiteral("Transport"));
}

void TestStore::groupsIncomingMoneyByCategory()
{
    SummaryModel summary;
    summary.setTransactions({
        make("1", QDate(2026, 9, 3), 250000, "Nordea", "Income", "Employer"),
        make("2", QDate(2026, 9, 10), 1500, "Nordea", "Refunds", "Shop"),
        make("3", QDate(2026, 9, 20), 50000, "Nordea", "Income", "Employer"),
        make("4", QDate(2026, 9, 21), -2490, "Aktia", "Transport"),
    });

    // Largest first, and the two salary rows land in one bucket.
    QCOMPARE(summary.income()->rowCount(), 2);
    const QModelIndex first = summary.income()->index(0, 0);
    QCOMPARE(summary.income()->data(first, CategoryTotalsModel::CategoryRole).toString(),
             QStringLiteral("Income"));
    QCOMPARE(summary.income()->data(first, CategoryTotalsModel::AmountMinorRole).toLongLong(),
             300000);
    QCOMPARE(summary.income()->data(first, CategoryTotalsModel::TransactionCountRole).toInt(), 2);

    // Spending is untouched by any of this.
    QCOMPARE(summary.totalSpentText(), Money::format(2490, "EUR"));
}

void TestStore::appliesTheSameExclusionsToIncome()
{
    SummaryModel summary;
    Transaction setAside = make("skip", QDate(2026, 9, 4), 9900, "Nordea", "Refunds");
    setAside.excluded = true;

    summary.setTransactions({
        make("pay", QDate(2026, 9, 3), 250000, "Nordea", "Income", "Employer"),
        setAside,
        // The incoming leg of moving money between your own accounts is not
        // income; counting it here would contradict the net card, which says
        // the same amount was left out.
        make("xfer", QDate(2026, 9, 5), 20000, "Aktia", "Transfers"),
        // A zero-amount row would otherwise add a category worth nothing.
        make("nil", QDate(2026, 9, 6), 0, "Nordea", "Adjustments"),
    });

    QCOMPARE(summary.income()->rowCount(), 1);
    QCOMPARE(summary.income()->data(summary.income()->index(0, 0),
                                    CategoryTotalsModel::CategoryRole)
                 .toString(),
             QStringLiteral("Income"));
}

void TestStore::countsExcludedTransfersOnlyOnce()
{
    SummaryModel summary;
    // A row that is both a transfer and hand-excluded belongs to one bucket
    // only, or the caption would report the same money twice.
    Transaction both = make("1", QDate(2026, 9, 1), -20000, "Aktia", "Transfers");
    both.excluded = true;

    summary.setTransactions({both, make("2", QDate(2026, 9, 2), -2490, "Aktia", "Transport")});

    QVERIFY(summary.hasExcluded());
    QVERIFY(!summary.hasTransfers());
    QCOMPARE(summary.excludedText(), Money::format(20000, "EUR"));
    QCOMPARE(summary.totalSpentText(), Money::format(2490, "EUR"));
}

void TestStore::neverSumsAcrossCurrencies()
{
    // Accounts in more than one country mean more than one currency. Adding
    // ore to cents produces a number that is not money in any currency, so the
    // summary must aggregate one currency and say it skipped the rest.
    SummaryModel summary;
    summary.setTransactions({
        make("1", QDate(2026, 9, 1), -1000, "Nordea", "Groceries", "Shop", "EUR"),
        make("2", QDate(2026, 9, 2), -2000, "Nordea", "Transport", "Shop", "EUR"),
        make("3", QDate(2026, 9, 3), -50000, "SEB", "Groceries", "Butik", "SEK"),
        make("4", QDate(2026, 9, 4), 400000, "SEB", "Income", "Lon", "SEK"),
    });

    // EUR has the most rows in this range, so it is what the totals describe.
    QCOMPARE(summary.currency(), QStringLiteral("EUR"));
    QCOMPARE(summary.totalSpentText(), Money::format(3000, "EUR"));
    QCOMPARE(summary.totalReceivedText(), Money::format(0, "EUR"));
    QVERIFY(summary.hasOtherCurrencies());

    // The SEK grocery spend must not inflate the EUR grocery bucket.
    QCOMPARE(summary.rowCount(), 2);
    for (int row = 0; row < summary.rowCount(); ++row) {
        const qint64 amount =
            summary.data(summary.index(row, 0), SummaryModel::AmountMinorRole).toLongLong();
        QVERIFY(amount < 50000);
    }
}

void TestStore::filtersByCurrency()
{
    TransactionStore store;
    QVERIFY(store.open(m_directory.filePath(QStringLiteral("g.sqlite"))));

    store.upsert({
        make("1", QDate(2026, 9, 1), -1000, "Nordea", "Groceries", "Shop", "EUR"),
        make("2", QDate(2026, 9, 2), -2000, "Nordea", "Transport", "Shop", "EUR"),
        make("3", QDate(2026, 9, 3), -48500, "SEB", "Groceries", "ICA", "SEK"),
    });

    // Most-used first, so the caller can default to it without guessing.
    QCOMPARE(store.currencies(), QStringList({"EUR", "SEK"}));

    TransactionStore::Filter swedish;
    swedish.currency = QStringLiteral("SEK");
    const TransactionList rows = store.query(swedish);
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().amountMinor, -48500LL);

    // Aggregating that slice reports SEK, with nothing skipped.
    SummaryModel summary;
    summary.setTransactions(rows);
    QCOMPARE(summary.currency(), QStringLiteral("SEK"));
    QCOMPARE(summary.totalSpentText(), Money::format(48500, "SEK"));
    QVERIFY(!summary.hasOtherCurrencies());
}

QTEST_GUILESS_MAIN(TestStore)
#include "tst_store.moc"
