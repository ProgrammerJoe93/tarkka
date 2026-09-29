#include "core/Budgets.h"
#include "core/Money.h"
#include "data/SummaryModel.h"
#include "data/TrendsModel.h"

#include <QTemporaryDir>
#include <QTest>

/*!
 * Monthly spending targets.
 *
 * The distinction that runs through all of this is between a category with no
 * budget and one budgeted at zero. The first is a category nobody has thought
 * about; the second is a decision to spend nothing on it. Collapsing them would
 * either colour every unbudgeted category as overspent or quietly ignore a
 * deliberate zero.
 */
class TestBudgets : public QObject
{
    Q_OBJECT

private slots:
    void distinguishesNoBudgetFromZero();
    void roundTripsThroughAFile();
    void treatsAMissingFileAsNoBudgets();
    void reportsProgressAgainstTheTarget();
    void treatsAnyoutlayAgainstAZeroTargetAsOver();
    void suggestsTheMedianMonth();
    void suggestsTheMeanWhenTheMedianIsNothing();

private:
    static Transaction spend(const QString &id, qint64 amountMinor, const QString &category);

    QTemporaryDir m_directory;
};

Transaction TestBudgets::spend(const QString &id, qint64 amountMinor, const QString &category)
{
    Transaction transaction;
    transaction.id = id;
    transaction.source = QStringLiteral("csv");
    transaction.institution = QStringLiteral("Aktia");
    transaction.bookingDate = QDate(2026, 9, 4);
    transaction.amountMinor = amountMinor;
    transaction.currency = QStringLiteral("EUR");
    transaction.category = category;
    transaction.status = QStringLiteral("BOOK");
    return transaction;
}

void TestBudgets::distinguishesNoBudgetFromZero()
{
    Budgets budgets;
    QVERIFY(!budgets.has(QStringLiteral("Groceries")));
    QCOMPARE(budgets.target(QStringLiteral("Groceries")), -1);

    budgets.setTarget(QStringLiteral("Groceries"), 0);
    QVERIFY(budgets.has(QStringLiteral("Groceries")));
    QCOMPARE(budgets.target(QStringLiteral("Groceries")), 0);

    // A negative clears rather than storing nonsense.
    budgets.setTarget(QStringLiteral("Groceries"), -1);
    QVERIFY(!budgets.has(QStringLiteral("Groceries")));
}

void TestBudgets::roundTripsThroughAFile()
{
    const QString path = m_directory.filePath(QStringLiteral("budgets.json"));

    Budgets written;
    written.setTarget(QStringLiteral("Groceries"), 34000);
    written.setTarget(QStringLiteral("Hobbies"), 0);
    QString errorMessage;
    QVERIFY2(written.saveToFile(path, &errorMessage), qPrintable(errorMessage));

    Budgets read;
    QVERIFY2(read.loadFromFile(path, &errorMessage), qPrintable(errorMessage));
    QCOMPARE(read.target(QStringLiteral("Groceries")), 34000);
    // The deliberate zero has to survive the round trip as a zero, not vanish.
    QVERIFY(read.has(QStringLiteral("Hobbies")));
    QCOMPARE(read.target(QStringLiteral("Hobbies")), 0);
}

void TestBudgets::treatsAMissingFileAsNoBudgets()
{
    Budgets budgets;
    QString errorMessage;
    // Nobody has set one yet: the starting state, not a failure to report.
    QVERIFY2(budgets.loadFromFile(m_directory.filePath(QStringLiteral("absent.json")),
                                  &errorMessage),
             qPrintable(errorMessage));
    QVERIFY(budgets.isEmpty());
}

void TestBudgets::reportsProgressAgainstTheTarget()
{
    SummaryModel summary;
    summary.setTransactions({spend("1", -15000, "Groceries"), spend("2", -5000, "Transport")});
    summary.setTargets({{QStringLiteral("Groceries"), 20000}});

    const QModelIndex groceries = summary.index(0, 0);
    QCOMPARE(summary.data(groceries, SummaryModel::CategoryRole).toString(),
             QStringLiteral("Groceries"));
    QCOMPARE(summary.data(groceries, SummaryModel::TargetTextRole).toString(),
             Money::format(20000, QStringLiteral("EUR")));
    QCOMPARE(summary.data(groceries, SummaryModel::TargetShareRole).toDouble(), 0.75);
    QVERIFY(!summary.data(groceries, SummaryModel::OverTargetRole).toBool());

    // Transport has no budget, so it reports none rather than looking overspent.
    const QModelIndex transport = summary.index(1, 0);
    QVERIFY(summary.data(transport, SummaryModel::TargetTextRole).toString().isEmpty());
    QVERIFY(!summary.data(transport, SummaryModel::OverTargetRole).toBool());
}

void TestBudgets::treatsAnyoutlayAgainstAZeroTargetAsOver()
{
    SummaryModel summary;
    summary.setTransactions({spend("1", -500, "Hobbies")});
    // Budgeted at nothing: spending anything is over, and the share must not
    // divide by the target.
    summary.setTargets({{QStringLiteral("Hobbies"), 0}});

    const QModelIndex hobbies = summary.index(0, 0);
    QVERIFY(summary.data(hobbies, SummaryModel::OverTargetRole).toBool());
    QCOMPARE(summary.data(hobbies, SummaryModel::TargetShareRole).toDouble(), 1.0);
}

void TestBudgets::suggestsTheMedianMonth()
{
    TrendsModel model;
    const QList<QDate> months = {QDate(2026, 6, 1), QDate(2026, 7, 1), QDate(2026, 8, 1)};
    // One unusual month must not drag the suggestion with it, which is why this
    // is a median rather than a mean.
    model.setSpending({{QDate(2026, 6, 1), QStringLiteral("Groceries"), 30000},
                       {QDate(2026, 7, 1), QStringLiteral("Groceries"), 32000},
                       {QDate(2026, 8, 1), QStringLiteral("Groceries"), 200000}},
                      months, QStringLiteral("EUR"));

    QCOMPARE(model.data(model.index(0, 0), TrendsModel::SuggestedMinorRole).toLongLong(), 32000);
}

void TestBudgets::suggestsTheMeanWhenTheMedianIsNothing()
{
    TrendsModel model;
    const QList<QDate> months = {QDate(2026, 6, 1), QDate(2026, 7, 1), QDate(2026, 8, 1),
                                 QDate(2026, 9, 1)};
    // Bought once a quarter: the median month is zero, and suggesting a budget
    // of zero would be useless, so the cost is spread across the window.
    model.setSpending({{QDate(2026, 7, 1), QStringLiteral("Insurance"), 40000}}, months,
                      QStringLiteral("EUR"));

    QCOMPARE(model.data(model.index(0, 0), TrendsModel::SuggestedMinorRole).toLongLong(), 10000);
}

QTEST_GUILESS_MAIN(TestBudgets)
#include "tst_budgets.moc"
