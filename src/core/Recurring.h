#pragma once

#include "core/Transaction.h"

#include <QDate>
#include <QList>
#include <QString>

/*!
 * Finds payments that repeat on a monthly rhythm.
 *
 * Committed spending is the part of a budget you cannot flex, and it is exactly
 * the part that hides: a subscription nobody cancelled, a price that crept up,
 * an insurance premium paid so reliably it stopped being noticed. None of that
 * is visible in a list sorted by date.
 *
 * Detection is deliberately conservative. Guessing wrong in the generous
 * direction produces a list of coincidences that has to be read carefully to be
 * trusted, which defeats the point; missing a borderline case only means the
 * list is shorter than it could be.
 */
namespace Recurring {

struct Payment
{
    QString merchant;   //!< The counterparty text the run was matched on.
    QString category;   //!< From the most recent occurrence.
    qint64 typicalMinor = 0;  //!< Positive magnitude; the median occurrence.
    //! The newest occurrence's amount, for spotting a price that has risen.
    qint64 latestMinor = 0;
    QDate lastSeen;
    int occurrences = 0;
};

/*!
 * The monthly payments among \a transactions, costliest first.
 *
 * Only outgoing rows in \a currency are considered, and rows the user has
 * excluded are ignored, so this agrees with every other figure in the app about
 * which rows count.
 *
 * A run qualifies on three tests, all of which a genuine subscription passes
 * easily and a run of coincidental purchases at the same shop does not:
 * at least \a minimumOccurrences of them, spacing that is monthly give or take
 * a few days, and amounts that stay close to each other.
 */
QList<Payment> find(const TransactionList &transactions,
                    const QString &currency,
                    int minimumOccurrences = 3);

} // namespace Recurring
