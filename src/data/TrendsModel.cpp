#include "data/TrendsModel.h"

#include "core/Money.h"

#include <QHash>
#include <QLocale>

#include <algorithm>

namespace {

/*!
 * How \a latest compares with \a previous, as -1, 0 or +1.
 *
 * A month with nothing to compare against reports 0 rather than inventing a
 * direction, and so does a change too small to be worth a colour: rounding a
 * few cents into "spending is up" would cry wolf every month.
 */
int direction(qint64 latest, qint64 previous, bool havePrevious)
{
    if (!havePrevious)
        return 0;
    constexpr qint64 noiseFloor = 100;  //!< One unit of currency.
    if (qAbs(latest - previous) < noiseFloor)
        return 0;
    return latest > previous ? 1 : -1;
}

/*!
 * "+12%", "-100%", or an amount when there is no percentage to be had.
 *
 * Growth from nothing has no percentage -- every first purchase in a category
 * would otherwise be an infinite rise -- so those report the amount instead.
 */
QString changeText(qint64 latest, qint64 previous, bool havePrevious, const QString &currency)
{
    if (!havePrevious)
        return {};
    if (previous == 0) {
        if (latest == 0)
            return {};
        return QStringLiteral("+%1").arg(Money::format(latest, currency));
    }

    const double ratio = static_cast<double>(latest - previous) / static_cast<double>(previous);
    const int percent = qRound(ratio * 100.0);
    if (percent == 0)
        return {};
    return QStringLiteral("%1%2%").arg(percent > 0 ? QStringLiteral("+") : QString())
        .arg(percent);
}

} // namespace

TrendsModel::TrendsModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void TrendsModel::setSpending(const QList<TransactionStore::MonthlySpend> &entries,
                              const QList<QDate> &months,
                              const QString &currency)
{
    beginResetModel();

    m_currency = currency;
    m_rows.clear();
    m_months.clear();
    m_monthTotals.assign(months.size(), 0);

    QHash<QDate, int> columnOf;
    for (qsizetype i = 0; i < months.size(); ++i)
        columnOf.insert(months.at(i), static_cast<int>(i));

    QHash<QString, Row> byCategory;
    for (const TransactionStore::MonthlySpend &entry : entries) {
        const int column = columnOf.value(entry.month, -1);
        if (column < 0)
            continue;  // Outside the window; the caller asked for a wider span.

        Row &row = byCategory[entry.category];
        if (row.spentMinor.isEmpty()) {
            row.category = entry.category;
            row.spentMinor.assign(months.size(), 0);
        }
        row.spentMinor[column] += entry.spentMinor;
        m_monthTotals[column] += entry.spentMinor;
    }

    // The tallest month sets the scale for every bar, so the columns are
    // comparable with each other rather than each filling its own height.
    qint64 peak = 0;
    for (const qint64 total : m_monthTotals)
        peak = qMax(peak, total);

    const QLocale locale;
    for (qsizetype i = 0; i < months.size(); ++i) {
        m_months.append(QVariantMap{
            {QStringLiteral("label"),
             locale.standaloneMonthName(months.at(i).month(), QLocale::ShortFormat)},
            {QStringLiteral("amountText"), Money::format(m_monthTotals.at(i), m_currency)},
            {QStringLiteral("share"),
             peak > 0 ? static_cast<double>(m_monthTotals.at(i)) / static_cast<double>(peak) : 0.0},
        });
    }

    m_rows = byCategory.values();
    // Biggest in the most recent month first, since that is the month the user
    // is looking at; ties break on the name so the order is stable between runs.
    const qsizetype last = months.size() - 1;
    std::sort(m_rows.begin(), m_rows.end(), [last](const Row &a, const Row &b) {
        const qint64 aLatest = last >= 0 ? a.spentMinor.at(last) : 0;
        const qint64 bLatest = last >= 0 ? b.spentMinor.at(last) : 0;
        if (aLatest != bLatest)
            return aLatest > bLatest;
        return a.category.localeAwareCompare(b.category) < 0;
    });

    endResetModel();
    emit countChanged();
}

int TrendsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant TrendsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};

    const Row &row = m_rows.at(index.row());
    const qsizetype last = row.spentMinor.size() - 1;
    const qint64 latest = last >= 0 ? row.spentMinor.at(last) : 0;
    const bool havePrevious = row.spentMinor.size() >= 2;
    const qint64 previous = havePrevious ? row.spentMinor.at(last - 1) : 0;

    switch (role) {
    case CategoryRole:
        return row.category;
    case MonthlyRole: {
        // Scaled against this category's own worst month, so a small category
        // still shows its shape instead of a row of invisible slivers.
        qint64 peak = 0;
        for (const qint64 amount : row.spentMinor)
            peak = qMax(peak, amount);

        QVariantList monthly;
        for (const qint64 amount : row.spentMinor) {
            monthly.append(QVariantMap{
                {QStringLiteral("amountMinor"), QVariant::fromValue(amount)},
                {QStringLiteral("amountText"), Money::format(amount, m_currency)},
                {QStringLiteral("share"),
                 peak > 0 ? static_cast<double>(amount) / static_cast<double>(peak) : 0.0},
            });
        }
        return monthly;
    }
    case LatestTextRole:
        return Money::format(latest, m_currency);
    case ChangeTextRole:
        return changeText(latest, previous, havePrevious, m_currency);
    case ChangeDirectionRole:
        return direction(latest, previous, havePrevious);
    default:
        return {};
    }
}

QHash<int, QByteArray> TrendsModel::roleNames() const
{
    return {
        {CategoryRole, "category"},
        {MonthlyRole, "monthly"},
        {LatestTextRole, "latestText"},
        {ChangeTextRole, "changeText"},
        {ChangeDirectionRole, "changeDirection"},
    };
}

QString TrendsModel::latestTotalText() const
{
    return Money::format(m_monthTotals.isEmpty() ? 0 : m_monthTotals.constLast(), m_currency);
}

QString TrendsModel::totalChangeText() const
{
    if (m_monthTotals.size() < 2)
        return {};
    return changeText(m_monthTotals.constLast(), m_monthTotals.at(m_monthTotals.size() - 2), true,
                      m_currency);
}

int TrendsModel::totalChangeDirection() const
{
    if (m_monthTotals.size() < 2)
        return 0;
    return direction(m_monthTotals.constLast(), m_monthTotals.at(m_monthTotals.size() - 2), true);
}
