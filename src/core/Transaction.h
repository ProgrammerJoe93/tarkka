#pragma once

#include <QDate>
#include <QList>
#include <QString>
#include <QtGlobal>

/*!
 * A single normalised purchase or transfer.
 *
 * Every provider -- live API or CSV import -- converts its own wire format into
 * this shape, so nothing downstream needs to know which bank a row came from.
 */
struct Transaction
{
    //! Stable, provider-independent identity used to deduplicate on re-sync.
    QString id;
    //! Provider that produced the row, e.g. "enablebanking" or "csv".
    QString source;
    //! Bank / ASPSP display name, e.g. "Nordea" or "Revolut".
    QString institution;
    QString accountId;
    QString accountName;

    QDate bookingDate;
    QDate valueDate;

    //! Best available human-readable label for the row.
    QString description;
    //! The other party, when the bank reports one separately.
    QString counterparty;
    QString reference;

    //! Signed minor units. Negative means money left the account.
    qint64 amountMinor = 0;
    QString currency;

    QString category;
    /*!
     * Whether the provider chose the category rather than the rules.
     *
     * Some providers know things no merchant rule can: a Trading 212 deposit is
     * a transfer because of the action type, not because of who it was paid to.
     * Re-running the rules over such a row would replace a right answer with a
     * guess, so it is marked and left alone.
     */
    bool categoryFromProvider = false;
    //! "BOOK" for booked, "PDNG" for pending, or whatever the bank reported.
    QString status;

    /*!
     * Left out of the totals at the user's request.
     *
     * Distinct from the automatic transfer exclusion, which is inferred from
     * the category: this one is a deliberate judgement about a single row --
     * a reimbursed expense, a one-off that would distort the month -- and so
     * it survives re-sync the way a hand-set category does.
     */
    bool excluded = false;

    bool isSpend() const { return amountMinor < 0; }
    bool isPending() const { return status.compare(QLatin1String("BOOK"), Qt::CaseInsensitive) != 0; }
};

using TransactionList = QList<Transaction>;

/*!
 * Derives a stable id for a row.
 *
 * Banks are inconsistent about supplying a durable identifier, so we prefer the
 * one they give us and fall back to a hash of the fields that together identify
 * a row in practice. \a ordinal disambiguates genuinely identical same-day rows
 * (two identical coffees) which would otherwise collapse into one.
 */
QString makeTransactionId(const QString &source,
                          const QString &accountId,
                          const QString &bankReference,
                          const QDate &date,
                          qint64 amountMinor,
                          const QString &description,
                          int ordinal);
