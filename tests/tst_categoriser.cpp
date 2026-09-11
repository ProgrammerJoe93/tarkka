#include "core/Categoriser.h"

#include <QTemporaryDir>
#include <QTest>

class TestCategoriser : public QObject
{
    Q_OBJECT

private slots:
    void matchesKnownMerchants_data();
    void matchesKnownMerchants();
    void incomeFallsBackToIncome();
    void unknownSpendIsUncategorised();
    void roundTripsThroughFile();
    void firstMatchingRuleWins();
};

namespace {

Transaction spendOn(const QString &counterparty)
{
    Transaction transaction;
    transaction.counterparty = counterparty;
    transaction.amountMinor = -1000;
    return transaction;
}

} // namespace

void TestCategoriser::matchesKnownMerchants_data()
{
    QTest::addColumn<QString>("merchant");
    QTest::addColumn<QString>("category");

    QTest::newRow("k-market") << "K-MARKET KAMPPI" << "Groceries";
    QTest::newRow("lidl") << "Lidl Helsinki" << "Groceries";
    QTest::newRow("wolt") << "WOLT" << "Restaurants & Cafes";
    QTest::newRow("hsl") << "HSL Mobiili" << "Transport";
    QTest::newRow("spotify") << "Spotify AB" << "Subscriptions";
    QTest::newRow("apteekki") << "Yliopiston Apteekki" << "Health & Pharmacy";
    QTest::newRow("verkkokauppa") << "Verkkokauppa.com" << "Shopping";
}

void TestCategoriser::matchesKnownMerchants()
{
    QFETCH(QString, merchant);
    QFETCH(QString, category);

    Categoriser categoriser;
    QCOMPARE(categoriser.categorise(spendOn(merchant)), category);
}

void TestCategoriser::incomeFallsBackToIncome()
{
    Transaction transaction;
    transaction.counterparty = QStringLiteral("Some Employer Oy");
    transaction.amountMinor = 250000;

    Categoriser categoriser;
    QCOMPARE(categoriser.categorise(transaction), Categoriser::income());
}

void TestCategoriser::unknownSpendIsUncategorised()
{
    Categoriser categoriser;
    QCOMPARE(categoriser.categorise(spendOn(QStringLiteral("Zzz Unknown Vendor"))),
             Categoriser::uncategorised());
}

void TestCategoriser::roundTripsThroughFile()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("rules.json"));

    Categoriser original;
    QVERIFY(original.saveToFile(path));

    Categoriser loaded;
    loaded.setRules({});
    QCOMPARE(loaded.rules().size(), 0);

    QString errorMessage;
    QVERIFY2(loaded.loadFromFile(path, &errorMessage), qPrintable(errorMessage));
    QCOMPARE(loaded.rules().size(), original.rules().size());
    QCOMPARE(loaded.categorise(spendOn(QStringLiteral("Lidl"))), QStringLiteral("Groceries"));
}

void TestCategoriser::firstMatchingRuleWins()
{
    Categoriser categoriser;
    categoriser.setRules({
        {QStringLiteral("First"), QRegularExpression(QStringLiteral("shop"))},
        {QStringLiteral("Second"), QRegularExpression(QStringLiteral("shop"))},
    });
    QCOMPARE(categoriser.categorise(spendOn(QStringLiteral("corner shop"))),
             QStringLiteral("First"));
}

QTEST_APPLESS_MAIN(TestCategoriser)
#include "tst_categoriser.moc"
