#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QString>

/*!
 * Totals per category, largest first, for one direction of money.
 *
 * A passive container: SummaryModel fills it while walking the rows it is
 * already walking, so the currency, transfer and exclusion rules are applied in
 * exactly one place. Splitting the aggregation would let the incoming and
 * outgoing halves of the dashboard drift apart about which rows count.
 *
 * The roles match what CategoryBars binds to, so the same component draws
 * money coming in and money going out.
 */
class CategoryTotalsModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by SummaryModel")

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role {
        CategoryRole = Qt::UserRole + 1,
        AmountMinorRole,
        AmountTextRole,
        ShareRole,
        TransactionCountRole,
    };
    Q_ENUM(Role)

    struct Bucket
    {
        QString category;
        qint64 amountMinor = 0;  //!< Positive magnitude, whichever way it moved.
        int transactionCount = 0;
    };

    explicit CategoryTotalsModel(QObject *parent = nullptr);

    //! Replaces the contents; \a buckets need not be sorted.
    void setBuckets(QList<Bucket> buckets, const QString &currency);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void countChanged();

private:
    QList<Bucket> m_buckets;
    qint64 m_totalMinor = 0;
    QString m_currency;
};
