#pragma once

#include <QDate>
#include <QList>
#include <QString>
#include <QtGlobal>

/*!
 * One balance as a bank reports it, and the choice of which one to show.
 *
 * Banks report several balances per account and disagree about which they
 * report at all: a booked balance, an available one that already accounts for
 * pending card authorisations, an opening balance, sometimes all three. There
 * is no single "the balance", so one has to be chosen and the choice has to be
 * visible -- a number labelled only "balance" when the bank next door means
 * something different by it is worse than no number.
 */
namespace Balance {

struct Reported
{
    QString name;      //!< The bank's own label, e.g. "Booked balance".
    QString type;      //!< ISO 20022 code: CLBD, ITAV, CLAV and friends.
    qint64 amountMinor = 0;
    QString currency;
    QDate referenceDate;

    bool isValid() const { return !type.isEmpty() || !currency.isEmpty(); }
};

/*!
 * The balance worth showing out of everything \a reported for one account.
 *
 * Preference runs from what you could spend right now towards what has merely
 * been booked, because the question a budgeting app is asked is "how much have
 * I got", not "what has cleared". Anything unrecognised is still better than
 * nothing, so an unknown type is taken over an empty answer rather than
 * discarded.
 *
 * Returns an invalid Reported when \a reported is empty.
 */
Reported preferred(const QList<Reported> &reported);

//! The ISO 20022 codes preferred(), in the order it prefers them.
QStringList preferenceOrder();

} // namespace Balance
