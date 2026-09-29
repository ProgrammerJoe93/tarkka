#pragma once

#include "core/Categoriser.h"
#include "core/Transaction.h"

#include <QDate>
#include <QString>
#include <QStringList>

/*!
 * SQLite-backed persistence for normalised transactions.
 *
 * The store is the application's source of truth: providers hand it rows and
 * the UI reads exclusively from it, so a sync that fails part way through never
 * leaves the display in a half-populated state and the app stays usable offline.
 */
class TransactionStore
{
public:
    struct Filter
    {
        QDate from;
        QDate to;
        QString institution;   //!< Empty means all banks.
        QString category;      //!< Empty means all categories.
        QString currency;      //!< Empty means all currencies.
        QString searchText;    //!< Matched against description and counterparty.
        bool spendOnly = false;
    };

    TransactionStore();
    ~TransactionStore();

    //! Opens (and migrates) the database at \a path.
    bool open(const QString &path, QString *errorMessage = nullptr);
    bool isOpen() const { return m_open; }

    /*!
     * Inserts or refreshes \a transactions, keyed on Transaction::id.
     *
     * A row the bank later amends (pending becoming booked, a corrected amount)
     * overwrites the stored copy, but a user-assigned category is preserved.
     * Returns the number of rows that were new, or -1 on failure.
     */
    int upsert(const TransactionList &transactions, QString *errorMessage = nullptr);

    TransactionList query(const Filter &filter, QString *errorMessage = nullptr) const;

    //! One category's spending within one calendar month.
    struct MonthlySpend
    {
        QDate month;       //!< First day of the month the total covers.
        QString category;
        qint64 spentMinor = 0;  //!< Positive magnitude of money spent.
    };

    /*!
     * Spending per category per month, for comparing months against each other.
     *
     * Aggregated in SQL rather than by loading rows, because the comparison
     * spans far more months than the transaction list ever shows at once.
     *
     * Counts exactly what the dashboard's figures count, so the two cannot
     * disagree: outgoing rows only, in one \a currency, with transfers between
     * the user's own accounts and rows excluded by hand both left out.
     */
    QList<MonthlySpend> monthlySpend(const QDate &from,
                                     const QDate &to,
                                     const QString &currency,
                                     QString *errorMessage = nullptr) const;

    /*!
     * The currency most of \a institution's rows are in, or empty if it has none.
     *
     * Trading 212's cash endpoint reports an amount with no currency beside it,
     * and a balance shown in the wrong one is worse than none at all.
     */
    QString dominantCurrency(const QString &institution) const;

    //! How many rows sit in \a category, across all time rather than a filter.
    int countInCategory(const QString &category) const;

    QStringList institutions() const;
    QStringList categories() const;
    //! Distinct currencies, most-used first, so the caller can default sensibly.
    QStringList currencies() const;
    QDate earliestDate() const;

    //! Overrides the category for one row and marks it as user-set.
    bool setCategory(const QString &transactionId, const QString &category,
                     QString *errorMessage = nullptr);

    /*!
     * Re-applies \a categoriser to every row whose category was not set by hand.
     *
     * Returns how many rows changed, or -1 on failure. A hand-set category is
     * never touched: the user deciding one row outranks any rule, including a
     * rule they wrote afterwards.
     */
    int recategorise(const Categoriser &categoriser, QString *errorMessage = nullptr);

    //! Leaves one row out of the totals, or puts it back.
    bool setExcluded(const QString &transactionId, bool excluded,
                     QString *errorMessage = nullptr);

    bool removeInstitution(const QString &institution, QString *errorMessage = nullptr);

    /*!
     * Deletes every stored transaction.
     *
     * Bank connections are untouched: forgetting what was synced and forgetting
     * where to sync from are different intentions, and conflating them would
     * make a cleanup into a reconnection.
     */
    bool clear(QString *errorMessage = nullptr);

private:
    bool migrate(QString *errorMessage);

    QString m_connectionName;
    bool m_open = false;
};
