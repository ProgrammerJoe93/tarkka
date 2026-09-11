#include "data/AccountModel.h"

AccountModel::AccountModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void AccountModel::setAccounts(const QList<Account> &accounts)
{
    beginResetModel();
    m_accounts = accounts;
    endResetModel();
    emit countChanged();
}

void AccountModel::clear()
{
    setAccounts({});
}

int AccountModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_accounts.size());
}

QVariant AccountModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_accounts.size())
        return {};

    const Account &account = m_accounts.at(index.row());
    switch (role) {
    case UidRole:
        return account.uid;
    case SessionIdRole:
        return account.sessionId;
    case InstitutionRole:
        return account.institution;
    case NameRole:
        return account.name;
    case IbanRole:
        return account.iban;
    case CurrencyRole:
        return account.currency;
    case ProductRole:
        return account.product;
    default:
        return {};
    }
}

QHash<int, QByteArray> AccountModel::roleNames() const
{
    return {
        {UidRole, "uid"},
        {SessionIdRole, "sessionId"},
        {InstitutionRole, "institution"},
        {NameRole, "name"},
        {IbanRole, "iban"},
        {CurrencyRole, "currency"},
        {ProductRole, "product"},
    };
}
