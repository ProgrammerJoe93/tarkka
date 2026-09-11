#include "data/SummaryModel.h"

#include "core/Money.h"

#include <QHash>

#include <algorithm>

SummaryModel::SummaryModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void SummaryModel::setTransactions(const TransactionList &transactions,
                                   const QDate &from,
                                   const QDate &to)
{
    beginResetModel();

    QHash<QString, Bucket> byCategory;
    QHash<QDate, qint64> byDay;
    m_totalSpentMinor = 0;
    m_totalReceivedMinor = 0;
    m_transferredMinor = 0;
    m_transferCount = 0;
    m_currency.clear();

    for (const Transaction &transaction : transactions) {
        if (m_currency.isEmpty())
            m_currency = transaction.currency;

        /*
         * Money moved between the user's own accounts is not spending and not
         * income. With a credit card linked alongside the account that pays it
         * off, every repayment would otherwise show up twice -- as spend on the
         * bank account and as income on the card -- inflating both totals.
         */
        if (transaction.category.compare(Categoriser::transfers(), Qt::CaseInsensitive) == 0) {
            m_transferredMinor += qAbs(transaction.amountMinor);
            ++m_transferCount;
            continue;
        }

        if (transaction.amountMinor >= 0) {
            m_totalReceivedMinor += transaction.amountMinor;
            continue;
        }

        const qint64 spent = -transaction.amountMinor;
        m_totalSpentMinor += spent;

        Bucket &bucket = byCategory[transaction.category];
        bucket.category = transaction.category;
        bucket.spentMinor += spent;
        ++bucket.transactionCount;

        byDay[transaction.bookingDate] += spent;
    }

    buildDailySeries(byDay, from, to);

    m_buckets = byCategory.values();
    // Largest first, then by name. Without the tie-break, two categories with
    // equal totals would swap places between runs -- the bucket map has no
    // meaningful order -- and the dashboard would reshuffle for no reason.
    std::sort(m_buckets.begin(), m_buckets.end(), [](const Bucket &a, const Bucket &b) {
        if (a.spentMinor != b.spentMinor)
            return a.spentMinor > b.spentMinor;
        return a.category.localeAwareCompare(b.category) < 0;
    });

    endResetModel();
    emit countChanged();
}

void SummaryModel::buildDailySeries(const QHash<QDate, qint64> &byDay,
                                    const QDate &from,
                                    const QDate &to)
{
    m_dailySpend.clear();
    if (byDay.isEmpty() && (!from.isValid() || !to.isValid()))
        return;

    // Fall back to the span the data itself covers when no range was supplied.
    QList<QDate> days = byDay.keys();
    std::sort(days.begin(), days.end());
    const QDate first = from.isValid() ? from : days.constFirst();
    const QDate last = to.isValid() ? to : days.constLast();
    if (!first.isValid() || !last.isValid() || first > last)
        return;

    // A very long range would produce more bars than pixels; cap it so the
    // sparkline stays a sparkline rather than becoming a memory hog.
    constexpr qint64 maxDays = 400;
    if (first.daysTo(last) + 1 > maxDays)
        return;

    qint64 peak = 0;
    for (const qint64 amount : byDay)
        peak = qMax(peak, amount);

    m_dailySpend.reserve(static_cast<qsizetype>(first.daysTo(last) + 1));
    for (QDate day = first; day <= last; day = day.addDays(1)) {
        const qint64 amount = byDay.value(day, 0);
        m_dailySpend.append(QVariantMap{
            {QStringLiteral("date"), day},
            {QStringLiteral("amountMinor"), QVariant::fromValue(amount)},
            {QStringLiteral("amountText"), Money::format(amount, m_currency)},
            {QStringLiteral("share"),
             peak > 0 ? static_cast<double>(amount) / static_cast<double>(peak) : 0.0},
        });
    }
}

int SummaryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_buckets.size());
}

QVariant SummaryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_buckets.size())
        return {};

    const Bucket &bucket = m_buckets.at(index.row());
    switch (role) {
    case CategoryRole:
        return bucket.category;
    case AmountMinorRole:
        return QVariant::fromValue(bucket.spentMinor);
    case AmountTextRole:
        return Money::format(bucket.spentMinor, m_currency);
    case ShareRole:
        return m_totalSpentMinor > 0
                   ? static_cast<double>(bucket.spentMinor) / static_cast<double>(m_totalSpentMinor)
                   : 0.0;
    case TransactionCountRole:
        return bucket.transactionCount;
    default:
        return {};
    }
}

QHash<int, QByteArray> SummaryModel::roleNames() const
{
    return {
        {CategoryRole, "category"},
        {AmountMinorRole, "amountMinor"},
        {AmountTextRole, "amountText"},
        {ShareRole, "share"},
        {TransactionCountRole, "transactionCount"},
    };
}

QString SummaryModel::totalSpentText() const
{
    return Money::format(m_totalSpentMinor, m_currency);
}

QString SummaryModel::totalReceivedText() const
{
    return Money::format(m_totalReceivedMinor, m_currency);
}

QString SummaryModel::netText() const
{
    return Money::format(m_totalReceivedMinor - m_totalSpentMinor, m_currency);
}

QString SummaryModel::transfersText() const
{
    // Each leg is counted, so halve it to report the amount actually moved.
    return Money::format(m_transferredMinor / 2, m_currency);
}

QString SummaryModel::largestCategory() const
{
    return m_buckets.isEmpty() ? QString() : m_buckets.first().category;
}
