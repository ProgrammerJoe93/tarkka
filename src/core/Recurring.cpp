#include "core/Recurring.h"

#include "core/Categoriser.h"

#include <QHash>

#include <algorithm>
#include <numeric>

namespace {

/*!
 * The merchant text reduced to something two months apart will agree on.
 *
 * Card descriptors carry noise that changes every time -- a trailing reference,
 * a city, a date, varying case, punctuation and spacing -- so matching the raw
 * text finds almost nothing. Letters are kept and everything else becomes a
 * separator, which is what makes "NETFLIX.COM 4021" and "netflix com 6207" the
 * same key. Dropping punctuation instead would join the words in one and not
 * the other, and the two would never meet.
 */
QString merchantKey(const Transaction &transaction)
{
    const QString source = transaction.counterparty.isEmpty() ? transaction.description
                                                              : transaction.counterparty;
    QString key;
    key.reserve(source.size());
    for (const QChar c : source) {
        if (c.isLetter())
            key.append(c.toCaseFolded());
        else if (!key.isEmpty() && !key.endsWith(QLatin1Char(' ')))
            key.append(QLatin1Char(' '));
    }
    return key.trimmed();
}

qint64 median(QList<qint64> values)
{
    if (values.isEmpty())
        return 0;
    std::sort(values.begin(), values.end());
    const qsizetype middle = values.size() / 2;
    return values.size() % 2 == 1 ? values.at(middle)
                                  : (values.at(middle - 1) + values.at(middle)) / 2;
}

/*!
 * Whether \a dates fall roughly a month apart, every time.
 *
 * Months are not a fixed length and payment dates drift across weekends, so the
 * window is wide; but every gap has to qualify. One stray gap means the run is
 * not a standing payment, and accepting it would sweep up any merchant visited
 * often enough.
 */
bool isMonthlyRhythm(const QList<QDate> &dates)
{
    constexpr int shortestGap = 24;
    constexpr int longestGap = 38;
    for (qsizetype i = 1; i < dates.size(); ++i) {
        const qint64 gap = dates.at(i - 1).daysTo(dates.at(i));
        if (gap < shortestGap || gap > longestGap)
            return false;
    }
    return true;
}

/*!
 * Whether every amount sits close to \a typical.
 *
 * Proportional rather than absolute, so a large premium is allowed the same
 * latitude as a small one. Wide enough for a subscription that has been
 * repriced mid-run, narrow enough that a weekly shop -- where the amount is
 * different every time -- does not qualify.
 */
bool amountsAreConsistent(const QList<qint64> &amounts, qint64 typical)
{
    if (typical <= 0)
        return false;
    for (const qint64 amount : amounts) {
        const double ratio = static_cast<double>(amount) / static_cast<double>(typical);
        if (ratio < 0.7 || ratio > 1.4)
            return false;
    }
    return true;
}

} // namespace

QList<Recurring::Payment> Recurring::find(const TransactionList &transactions,
                                          const QString &currency,
                                          int minimumOccurrences)
{
    struct Run
    {
        QString merchant;
        QString category;
        QDate newest;
        QList<QDate> dates;
        QList<qint64> amounts;
    };

    QHash<QString, Run> byMerchant;
    for (const Transaction &transaction : transactions) {
        if (transaction.excluded || transaction.amountMinor >= 0)
            continue;
        if (!currency.isEmpty() && transaction.currency != currency)
            continue;
        // Moving money between your own accounts is regular by nature and is
        // not a payment to anyone; listing it as committed spending would be
        // double-counting the same money the net card already leaves out.
        if (transaction.category.compare(Categoriser::transfers(), Qt::CaseInsensitive) == 0)
            continue;

        const QString key = merchantKey(transaction);
        if (key.isEmpty())
            continue;

        Run &run = byMerchant[key];
        if (run.dates.isEmpty()) {
            run.merchant = transaction.counterparty.isEmpty() ? transaction.description
                                                              : transaction.counterparty;
        }
        run.dates.append(transaction.bookingDate);
        run.amounts.append(-transaction.amountMinor);
        // The newest occurrence supplies the category, since a recategorisation
        // should show up rather than being outvoted by history. Tracked as it
        // goes, because the rows arrive in whatever order the query returned.
        if (!run.newest.isValid() || transaction.bookingDate > run.newest) {
            run.newest = transaction.bookingDate;
            run.category = transaction.category;
        }
    }

    QList<Payment> payments;
    for (Run &run : byMerchant) {
        if (run.dates.size() < minimumOccurrences)
            continue;

        // Sort dates and amounts together, so "latest" really is the latest.
        QList<qsizetype> order(run.dates.size());
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(), [&run](qsizetype a, qsizetype b) {
            return run.dates.at(a) < run.dates.at(b);
        });
        QList<QDate> dates;
        QList<qint64> amounts;
        dates.reserve(order.size());
        amounts.reserve(order.size());
        for (const qsizetype i : order) {
            dates.append(run.dates.at(i));
            amounts.append(run.amounts.at(i));
        }

        if (!isMonthlyRhythm(dates))
            continue;

        const qint64 typical = median(amounts);
        if (!amountsAreConsistent(amounts, typical))
            continue;

        Payment payment;
        payment.merchant = run.merchant;
        payment.category = run.category;
        payment.typicalMinor = typical;
        payment.latestMinor = amounts.constLast();
        payment.lastSeen = dates.constLast();
        payment.occurrences = static_cast<int>(dates.size());
        payments.append(payment);
    }

    // Costliest first: that is the order in which they are worth reviewing.
    std::sort(payments.begin(), payments.end(), [](const Payment &a, const Payment &b) {
        if (a.typicalMinor != b.typicalMinor)
            return a.typicalMinor > b.typicalMinor;
        return a.merchant.localeAwareCompare(b.merchant) < 0;
    });
    return payments;
}
