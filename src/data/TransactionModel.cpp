#include "data/TransactionModel.h"

#include "core/Money.h"

#include <QLocale>

TransactionModel::TransactionModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void TransactionModel::setTransactions(const TransactionList &transactions)
{
    beginResetModel();
    m_transactions = transactions;
    endResetModel();
    emit countChanged();
}

int TransactionModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_transactions.size());
}

QVariant TransactionModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_transactions.size())
        return {};

    const Transaction &transaction = m_transactions.at(index.row());
    switch (role) {
    case IdRole:
        return transaction.id;
    case InstitutionRole:
        return transaction.institution;
    case AccountNameRole:
        return transaction.accountName;
    case DateRole:
        return transaction.bookingDate;
    case DateTextRole:
        return QLocale::system().toString(transaction.bookingDate, QLocale::ShortFormat);
    case DescriptionRole:
        return transaction.description;
    case CounterpartyRole:
        return transaction.counterparty;
    case ReferenceRole:
        return transaction.reference;
    case AmountMinorRole:
        return QVariant::fromValue(transaction.amountMinor);
    case AmountTextRole:
        return Money::format(transaction.amountMinor, transaction.currency);
    case CurrencyRole:
        return transaction.currency;
    case CategoryRole:
        return transaction.category;
    case IsSpendRole:
        return transaction.isSpend();
    case IsPendingRole:
        return transaction.isPending();
    default:
        return {};
    }
}

QHash<int, QByteArray> TransactionModel::roleNames() const
{
    return {
        {IdRole, "transactionId"},
        {InstitutionRole, "institution"},
        {AccountNameRole, "accountName"},
        {DateRole, "date"},
        {DateTextRole, "dateText"},
        {DescriptionRole, "description"},
        {CounterpartyRole, "counterparty"},
        {ReferenceRole, "reference"},
        {AmountMinorRole, "amountMinor"},
        {AmountTextRole, "amountText"},
        {CurrencyRole, "currency"},
        {CategoryRole, "category"},
        {IsSpendRole, "isSpend"},
        {IsPendingRole, "isPending"},
    };
}
