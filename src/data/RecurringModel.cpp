#include "data/RecurringModel.h"

#include "core/Money.h"

#include <QLocale>

RecurringModel::RecurringModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void RecurringModel::setPayments(const QList<Recurring::Payment> &payments,
                                 const QString &currency)
{
    beginResetModel();
    m_payments = payments;
    m_currency = currency;
    endResetModel();
    emit countChanged();
}

int RecurringModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_payments.size());
}

QVariant RecurringModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_payments.size())
        return {};

    const Recurring::Payment &payment = m_payments.at(index.row());
    switch (role) {
    case MerchantRole:
        return payment.merchant;
    case CategoryRole:
        return payment.category;
    case TypicalTextRole:
        return Money::format(payment.typicalMinor, m_currency);
    case LastSeenTextRole:
        return QLocale().toString(payment.lastSeen, QLocale::ShortFormat);
    case OccurrencesRole:
        return payment.occurrences;
    case PriceChangedRole:
        // A unit of currency, the same floor the trend colours use: rounding
        // pennies into "the price changed" would flag everything every month.
        return qAbs(payment.latestMinor - payment.typicalMinor) >= 100;
    case LatestTextRole:
        return Money::format(payment.latestMinor, m_currency);
    default:
        return {};
    }
}

QHash<int, QByteArray> RecurringModel::roleNames() const
{
    return {
        {MerchantRole, "merchant"},
        {CategoryRole, "category"},
        {TypicalTextRole, "typicalText"},
        {LastSeenTextRole, "lastSeenText"},
        {OccurrencesRole, "occurrences"},
        {PriceChangedRole, "priceChanged"},
        {LatestTextRole, "latestText"},
    };
}

QString RecurringModel::totalText() const
{
    qint64 total = 0;
    for (const Recurring::Payment &payment : m_payments)
        total += payment.typicalMinor;
    return Money::format(total, m_currency);
}
