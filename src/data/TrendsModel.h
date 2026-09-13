#pragma once

#include "data/TransactionStore.h"

#include <QAbstractListModel>
#include <QDate>
#include <QQmlEngine>
#include <QVariantList>

/*!
 * Spending compared across a run of months, by category.
 *
 * The dashboard answers "where did the money go this month"; this answers
 * "is that more or less than usual", which needs several months at once and so
 * cannot come from the same filtered slice.
 *
 * One row per category, ordered by what it cost in the most recent month, each
 * carrying its figure for every month in the window. Categories that have
 * disappeared still appear while they are inside the window, because a category
 * dropping to nothing is exactly the kind of change worth seeing.
 */
class TrendsModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by AppController")

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    //! One entry per month in the window: {label, amountText, share}.
    Q_PROPERTY(QVariantList months READ months NOTIFY countChanged)
    Q_PROPERTY(QString currency READ currency NOTIFY countChanged)
    Q_PROPERTY(bool hasData READ hasData NOTIFY countChanged)

    //! The latest month's total, and how it compares with the month before.
    Q_PROPERTY(QString latestTotalText READ latestTotalText NOTIFY countChanged)
    Q_PROPERTY(QString totalChangeText READ totalChangeText NOTIFY countChanged)
    Q_PROPERTY(int totalChangeDirection READ totalChangeDirection NOTIFY countChanged)

public:
    enum Role {
        CategoryRole = Qt::UserRole + 1,
        //! Per-month figures for this category: {amountMinor, amountText, share}.
        MonthlyRole,
        LatestTextRole,
        ChangeTextRole,
        /*!
         * -1, 0 or +1: spending down, flat, or up against the previous month.
         *
         * Not a bool, because "no earlier month to compare with" and "the same
         * as last month" are different statements and a colour that implied
         * either would be a lie.
         */
        ChangeDirectionRole,
    };
    Q_ENUM(Role)

    explicit TrendsModel(QObject *parent = nullptr);

    /*!
     * Rebuilds from \a entries, which must cover the months in \a months.
     *
     * \a months is supplied rather than derived so that a month with no
     * spending at all still occupies a column instead of silently closing the
     * gap and making the run of months look continuous when it is not.
     */
    void setSpending(const QList<TransactionStore::MonthlySpend> &entries,
                     const QList<QDate> &months,
                     const QString &currency);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QVariantList months() const { return m_months; }
    QString currency() const { return m_currency; }
    bool hasData() const { return !m_rows.isEmpty(); }
    QString latestTotalText() const;
    QString totalChangeText() const;
    int totalChangeDirection() const;

signals:
    void countChanged();

private:
    struct Row
    {
        QString category;
        QList<qint64> spentMinor;  //!< One per month, in window order.
    };

    QList<Row> m_rows;
    QVariantList m_months;
    QList<qint64> m_monthTotals;
    QString m_currency;
};
