#pragma once

#include "core/Categoriser.h"
#include "core/Transaction.h"

#include <QAbstractListModel>
#include <QDate>
#include <QHash>
#include <QVariantList>
#include <QQmlEngine>

/*!
 * Per-category spend totals for the dashboard, largest first.
 *
 * Only outgoing rows are aggregated -- mixing income in would make the shares
 * meaningless -- and the totals are recomputed from whatever slice of
 * transactions the dashboard is currently showing.
 */
class SummaryModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by AppController")

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QString totalSpentText READ totalSpentText NOTIFY countChanged)
    Q_PROPERTY(QString totalReceivedText READ totalReceivedText NOTIFY countChanged)
    Q_PROPERTY(QString netText READ netText NOTIFY countChanged)
    Q_PROPERTY(QString largestCategory READ largestCategory NOTIFY countChanged)
    Q_PROPERTY(QVariantList dailySpend READ dailySpend NOTIFY countChanged)
    Q_PROPERTY(QString transfersText READ transfersText NOTIFY countChanged)
    Q_PROPERTY(bool hasTransfers READ hasTransfers NOTIFY countChanged)
    Q_PROPERTY(QString currency READ currency NOTIFY countChanged)
    Q_PROPERTY(bool hasOtherCurrencies READ hasOtherCurrencies NOTIFY countChanged)
    Q_PROPERTY(int otherCurrencyCount READ otherCurrencyCount NOTIFY countChanged)

public:
    enum Role {
        CategoryRole = Qt::UserRole + 1,
        AmountMinorRole,
        AmountTextRole,
        ShareRole,
        TransactionCountRole,
    };
    Q_ENUM(Role)

    explicit SummaryModel(QObject *parent = nullptr);

    //! \a from and \a to bound the daily series so empty days still appear.
    void setTransactions(const TransactionList &transactions,
                         const QDate &from = {},
                         const QDate &to = {});

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString totalSpentText() const;
    QString totalReceivedText() const;
    QString netText() const;
    QString largestCategory() const;

    //! Total moved between the user's own accounts, excluded from the figures above.
    QString transfersText() const;
    bool hasTransfers() const { return m_transferCount > 0; }

    /*!
     * The currency every figure in this model is expressed in.
     *
     * Minor units are only comparable within one currency -- adding ore to
     * cents yields a number that is not money at all -- so the model picks the
     * currency carrying the most rows and reports how many it left out. In
     * normal use the filter has already narrowed the data to one currency and
     * nothing is skipped; this is what keeps a mistake visible instead of
     * silently producing a wrong total.
     */
    QString currency() const { return m_currency; }
    bool hasOtherCurrencies() const { return m_otherCurrencyCount > 0; }
    int otherCurrencyCount() const { return m_otherCurrencyCount; }

    /*!
     * One entry per day in the range: {"date", "amountMinor", "share"}.
     *
     * \c share is relative to the busiest day, which is what the sparkline
     * needs; days with no spending are present with a zero amount so the
     * spacing stays true to the calendar.
     */
    QVariantList dailySpend() const { return m_dailySpend; }

signals:
    void countChanged();

private:
    void buildDailySeries(const QHash<QDate, qint64> &byDay, const QDate &from, const QDate &to);

    struct Bucket
    {
        QString category;
        qint64 spentMinor = 0;  //!< Positive magnitude of money spent.
        int transactionCount = 0;
    };

    QList<Bucket> m_buckets;
    QVariantList m_dailySpend;
    qint64 m_totalSpentMinor = 0;
    qint64 m_totalReceivedMinor = 0;
    qint64 m_transferredMinor = 0;
    int m_transferCount = 0;
    int m_otherCurrencyCount = 0;
    QString m_currency;
};
