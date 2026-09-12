#pragma once

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

    QStringList institutions() const;
    QStringList categories() const;
    //! Distinct currencies, most-used first, so the caller can default sensibly.
    QStringList currencies() const;
    QDate earliestDate() const;

    //! Overrides the category for one row and marks it as user-set.
    bool setCategory(const QString &transactionId, const QString &category,
                     QString *errorMessage = nullptr);

    bool removeInstitution(const QString &institution, QString *errorMessage = nullptr);

private:
    bool migrate(QString *errorMessage);

    QString m_connectionName;
    bool m_open = false;
};
