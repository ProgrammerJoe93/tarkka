#include "data/CategoryTotalsModel.h"

#include "core/Money.h"

#include <algorithm>

CategoryTotalsModel::CategoryTotalsModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void CategoryTotalsModel::setBuckets(QList<Bucket> buckets, const QString &currency)
{
    beginResetModel();

    m_currency = currency;
    m_buckets = std::move(buckets);
    m_totalMinor = 0;
    for (const Bucket &bucket : m_buckets)
        m_totalMinor += bucket.amountMinor;

    // Largest first, then by name. Without the tie-break two equal categories
    // would swap places between runs and the list would reshuffle for no
    // reason, since the map they came from has no meaningful order.
    std::sort(m_buckets.begin(), m_buckets.end(), [](const Bucket &a, const Bucket &b) {
        if (a.amountMinor != b.amountMinor)
            return a.amountMinor > b.amountMinor;
        return a.category.localeAwareCompare(b.category) < 0;
    });

    endResetModel();
    emit countChanged();
}

int CategoryTotalsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_buckets.size());
}

QVariant CategoryTotalsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_buckets.size())
        return {};

    const Bucket &bucket = m_buckets.at(index.row());
    switch (role) {
    case CategoryRole:
        return bucket.category;
    case AmountMinorRole:
        return QVariant::fromValue(bucket.amountMinor);
    case AmountTextRole:
        return Money::format(bucket.amountMinor, m_currency);
    case ShareRole:
        return m_totalMinor > 0
                   ? static_cast<double>(bucket.amountMinor) / static_cast<double>(m_totalMinor)
                   : 0.0;
    case TransactionCountRole:
        return bucket.transactionCount;
    default:
        return {};
    }
}

QHash<int, QByteArray> CategoryTotalsModel::roleNames() const
{
    return {
        {CategoryRole, "category"},
        {AmountMinorRole, "amountMinor"},
        {AmountTextRole, "amountText"},
        {ShareRole, "share"},
        {TransactionCountRole, "transactionCount"},
    };
}
