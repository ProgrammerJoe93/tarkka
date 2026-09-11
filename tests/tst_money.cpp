#include "core/Money.h"

#include <QTest>

class TestMoney : public QObject
{
    Q_OBJECT

private slots:
    void parsesDecimalForms_data();
    void parsesDecimalForms();
    void rejectsGarbage();
    void formatsWithCurrency();
};

void TestMoney::parsesDecimalForms_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<qint64>("expected");

    QTest::newRow("plain") << "12.34" << 1234LL;
    QTest::newRow("negative") << "-12.34" << -1234LL;
    QTest::newRow("nordic comma") << "12,34" << 1234LL;
    QTest::newRow("nordic grouped") << "1 234,56" << 123456LL;
    QTest::newRow("anglo grouped") << "1,234.56" << 123456LL;
    QTest::newRow("nbsp grouped") << QString("1 234,56") << 123456LL;
    QTest::newRow("integer") << "42" << 4200LL;
    QTest::newRow("lone comma as grouping") << "1,234" << 123400LL;
    QTest::newRow("leading plus") << "+9.99" << 999LL;
    QTest::newRow("rounds half up") << "0.005" << 1LL;
    QTest::newRow("whitespace") << "  7.50  " << 750LL;
}

void TestMoney::parsesDecimalForms()
{
    QFETCH(QString, text);
    QFETCH(qint64, expected);

    bool ok = false;
    QCOMPARE(Money::fromDecimalString(text, &ok), expected);
    QVERIFY(ok);
}

void TestMoney::rejectsGarbage()
{
    bool ok = true;
    Money::fromDecimalString(QStringLiteral("not a number"), &ok);
    QVERIFY(!ok);

    ok = true;
    Money::fromDecimalString(QString(), &ok);
    QVERIFY(!ok);
}

void TestMoney::formatsWithCurrency()
{
    QVERIFY(Money::format(123456, QStringLiteral("EUR")).endsWith(QStringLiteral("EUR")));
    QVERIFY(Money::format(-500, QString()).contains(QLatin1Char('5')));
}

QTEST_APPLESS_MAIN(TestMoney)
#include "tst_money.moc"
