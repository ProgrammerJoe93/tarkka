#include "data/TransactionFilterModel.h"

#include "data/TransactionModel.h"

TransactionFilterModel::TransactionFilterModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    connect(this, &QAbstractItemModel::rowsInserted, this, &TransactionFilterModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &TransactionFilterModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &TransactionFilterModel::countChanged);
}

void TransactionFilterModel::setSearchText(const QString &text)
{
    if (m_searchText == text)
        return;
    m_searchText = text;
    invalidateRowsFilter();
    emit searchTextChanged();
    emit countChanged();
}

bool TransactionFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (m_searchText.isEmpty())
        return true;

    const QAbstractItemModel *source = sourceModel();
    if (!source)
        return true;

    const QModelIndex index = source->index(sourceRow, 0, sourceParent);
    for (int role : {TransactionModel::DescriptionRole,
                     TransactionModel::CounterpartyRole,
                     TransactionModel::CategoryRole,
                     TransactionModel::ReferenceRole,
                     TransactionModel::InstitutionRole}) {
        if (source->data(index, role).toString().contains(m_searchText, Qt::CaseInsensitive))
            return true;
    }
    return false;
}
