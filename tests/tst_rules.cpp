#include "core/Categoriser.h"

#include <QTemporaryDir>
#include <QTest>

/*!
 * Category rules, and the two things about them that are easy to get wrong.
 *
 * Order is precedence, so it has to survive being written to disk and read
 * back; and adding a rule for a merchant that already has one has to replace
 * it, or two rules disagree and position alone decides which is believed.
 */
class TestRules : public QObject
{
    Q_OBJECT

private slots:
    void keepsRuleOrderAcrossAFileRoundTrip();
    void replacesARuleForTheSameMerchant();
    void aNewRuleOutranksTheOneItCorrects();
    void stillReadsTheOlderFileFormat();

private:
    static Categoriser::Rule rule(const QString &category, const QString &pattern);
    static Transaction rowFor(const QString &merchant);

    QTemporaryDir m_directory;
};

Categoriser::Rule TestRules::rule(const QString &category, const QString &pattern)
{
    return {category, QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption)};
}

Transaction TestRules::rowFor(const QString &merchant)
{
    Transaction transaction;
    transaction.counterparty = merchant;
    transaction.amountMinor = -1000;
    return transaction;
}

void TestRules::keepsRuleOrderAcrossAFileRoundTrip()
{
    const QString path = m_directory.filePath(QStringLiteral("rules.json"));

    Categoriser written;
    // Two rules matching the same text. The first wins, and "Zebra" is first
    // only because it was put there -- a format keyed by category would hand
    // these back sorted and quietly make "Apple" the answer instead.
    written.setRules({rule(QStringLiteral("Zebra"), QStringLiteral("puutarha")),
                      rule(QStringLiteral("Apple"), QStringLiteral("puutarha"))});
    QCOMPARE(written.categorise(rowFor(QStringLiteral("PUUTARHA OY"))), QStringLiteral("Zebra"));

    QString errorMessage;
    QVERIFY2(written.saveToFile(path, &errorMessage), qPrintable(errorMessage));

    Categoriser reloaded;
    QVERIFY2(reloaded.loadFromFile(path, &errorMessage), qPrintable(errorMessage));
    QCOMPARE(reloaded.categorise(rowFor(QStringLiteral("PUUTARHA OY"))), QStringLiteral("Zebra"));
    QCOMPARE(reloaded.rules().size(), 2);
}

void TestRules::replacesARuleForTheSameMerchant()
{
    Categoriser categoriser;
    categoriser.setRules({});

    categoriser.setRule(rule(QStringLiteral("Hobbies"), QStringLiteral("puutarha")));
    categoriser.setRule(rule(QStringLiteral("Gifts"), QStringLiteral("puutarha")));

    // Changing your mind about a merchant updates the answer; it does not
    // leave two rules behind for position to arbitrate between.
    QCOMPARE(categoriser.rules().size(), 1);
    QCOMPARE(categoriser.categorise(rowFor(QStringLiteral("PUUTARHA OY"))),
             QStringLiteral("Gifts"));
}

void TestRules::aNewRuleOutranksTheOneItCorrects()
{
    Categoriser categoriser;
    categoriser.setRules({rule(QStringLiteral("Shopping"), QStringLiteral("oy\\b"))});

    // A different pattern, so both are kept -- but the new one is the
    // correction and has to be consulted first.
    categoriser.setRule(rule(QStringLiteral("Hobbies"), QStringLiteral("puutarha")));

    QCOMPARE(categoriser.rules().size(), 2);
    QCOMPARE(categoriser.categorise(rowFor(QStringLiteral("PUUTARHA OY"))),
             QStringLiteral("Hobbies"));
}

void TestRules::stillReadsTheOlderFileFormat()
{
    // Files written by earlier versions, and by hand, are out there.
    const QString path = m_directory.filePath(QStringLiteral("legacy.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(R"({"Hobbies": ["puutarha", "kukka"], "Transport": ["hsl"]})");
    file.close();

    Categoriser categoriser;
    QString errorMessage;
    QVERIFY2(categoriser.loadFromFile(path, &errorMessage), qPrintable(errorMessage));
    QCOMPARE(categoriser.rules().size(), 3);
    QCOMPARE(categoriser.categorise(rowFor(QStringLiteral("PUUTARHA OY"))),
             QStringLiteral("Hobbies"));
    QCOMPARE(categoriser.categorise(rowFor(QStringLiteral("HSL"))), QStringLiteral("Transport"));
}

QTEST_APPLESS_MAIN(TestRules)
#include "tst_rules.moc"
