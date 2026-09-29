#include "core/Balance.h"

#include <QTest>

/*!
 * Choosing which balance to show.
 *
 * Banks report several per account and disagree about which they report at
 * all, so there is no "the balance" to read off. The choice has to favour the
 * one that answers the question a budgeting app is asked -- how much could I
 * spend -- and has to degrade rather than fail when a bank reports something
 * unexpected.
 */
class TestBalance : public QObject
{
    Q_OBJECT

private slots:
    void prefersWhatCouldBeSpentNow();
    void fallsBackThroughBookedBalances();
    void takesAnUnknownTypeOverNothing();
    void reportsNothingWhenTheBankSentNothing();
    void matchesTheTypeCaseInsensitively();

private:
    static Balance::Reported of(const QString &type, qint64 minor);
};

Balance::Reported TestBalance::of(const QString &type, qint64 minor)
{
    Balance::Reported reported;
    reported.type = type;
    reported.amountMinor = minor;
    reported.currency = QStringLiteral("EUR");
    reported.name = type;
    return reported;
}

void TestBalance::prefersWhatCouldBeSpentNow()
{
    // The booked balance ignores card authorisations that have not settled, so
    // it reads high on the day you have used the card. The interim available
    // one is the money actually at hand.
    const auto chosen = Balance::preferred({of(QStringLiteral("CLBD"), 50000),
                                            of(QStringLiteral("ITAV"), 42000)});
    QCOMPARE(chosen.type, QStringLiteral("ITAV"));
    QCOMPARE(chosen.amountMinor, 42000);
}

void TestBalance::fallsBackThroughBookedBalances()
{
    // A bank reporting no available balance at all still has a usable answer.
    const auto chosen = Balance::preferred({of(QStringLiteral("OPBD"), 10000),
                                            of(QStringLiteral("CLBD"), 20000)});
    QCOMPARE(chosen.type, QStringLiteral("CLBD"));
}

void TestBalance::takesAnUnknownTypeOverNothing()
{
    // A non-standard code is still a balance, and showing it beats a blank.
    const auto chosen = Balance::preferred({of(QStringLiteral("WEIRD"), 1234)});
    QVERIFY(chosen.isValid());
    QCOMPARE(chosen.amountMinor, 1234);
}

void TestBalance::reportsNothingWhenTheBankSentNothing()
{
    // Not every bank serves balances; that is not a zero balance.
    QVERIFY(!Balance::preferred({}).isValid());
}

void TestBalance::matchesTheTypeCaseInsensitively()
{
    const auto chosen = Balance::preferred({of(QStringLiteral("clbd"), 900),
                                            of(QStringLiteral("itav"), 800)});
    QCOMPARE(chosen.amountMinor, 800);
}

QTEST_APPLESS_MAIN(TestBalance)
#include "tst_balance.moc"
