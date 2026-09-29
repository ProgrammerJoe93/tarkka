#include "data/AccountModel.h"

#include "core/Money.h"

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

void AccountModel::setBalance(const QString &uid, const Account &balance)
{
    for (qsizetype i = 0; i < m_accounts.size(); ++i) {
        if (m_accounts.at(i).uid != uid)
            continue;

        Account &account = m_accounts[i];
        account.hasBalance = balance.hasBalance;
        account.balanceMinor = balance.balanceMinor;
        account.balanceCurrency = balance.balanceCurrency;
        account.balanceName = balance.balanceName;
        account.balanceType = balance.balanceType;
        account.balanceDate = balance.balanceDate;

        const QModelIndex changed = index(static_cast<int>(i), 0);
        emit dataChanged(changed, changed,
                         {HasBalanceRole, BalanceTextRole, BalanceMinorRole, BalanceNameRole});
        emit countChanged();
        return;
    }
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
    case IdentifierRole:
        return account.identifier;
    case CurrencyRole:
        return account.currency;
    case ProductRole:
        return account.product;
    case CashAccountTypeRole:
        return account.cashAccountType;
    case IsCardRole:
        return account.isCard();
    case HasBalanceRole:
        return account.hasBalance;
    case BalanceTextRole:
        return account.hasBalance ? Money::format(account.balanceMinor, account.balanceCurrency)
                                  : QString();
    case BalanceMinorRole:
        return QVariant::fromValue(account.balanceMinor);
    case BalanceNameRole:
        return account.balanceName;
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
        {IdentifierRole, "identifier"},
        {CurrencyRole, "currency"},
        {ProductRole, "product"},
        {CashAccountTypeRole, "cashAccountType"},
        {IsCardRole, "isCard"},
        {HasBalanceRole, "hasBalance"},
        {BalanceTextRole, "balanceText"},
        {BalanceMinorRole, "balanceMinor"},
        {BalanceNameRole, "balanceName"},
    };
}
