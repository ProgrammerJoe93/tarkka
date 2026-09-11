#include "data/SummaryModel.h"
#include "data/TransactionStore.h"

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

private:
    Transaction make(const QString &id, const QDate &date, qint64 amount,
                     const QString &institution, const QString &category,
                     const QString &counterparty = QStringLiteral("Shop")) const;

    QTemporaryDir m_directory;
};

Transaction TestStore::make(const QString &id, const QDate &date, qint64 amount,
                            const QString &institution, const QString &category,
                            const QString &counterparty) const
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
    transaction.currency = QStringLiteral("EUR");
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
    // Largest bucket first: Groceries 30.00 beats Transport 30.00? No -- 3000 each.
    QCOMPARE(summary.largestCategory(), QStringLiteral("Groceries"));
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

QTEST_GUILESS_MAIN(TestStore)
#include "tst_store.moc"
