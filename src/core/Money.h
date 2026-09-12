#pragma once

#include <QLocale>
#include <QString>
#include <QtGlobal>

#include <cmath>

/*!
 * Monetary helpers.
 *
 * Money is carried through the whole application as a signed integer count of
 * minor units (cents), never as a floating point value. Negative means money
 * left the account. Parsing and formatting are the only places where a decimal
 * representation exists.
 */
namespace Money {

inline qint64 fromDecimalString(QString text, bool *ok = nullptr)
{
    text = text.trimmed();
    if (text.isEmpty()) {
        if (ok)
            *ok = false;
        return 0;
    }

    // Accept both "1 234,56" (Nordic) and "1,234.56" (Anglo) groupings.
    text.remove(QLatin1Char(' '));
    text.remove(QChar(0x00A0));  // non-breaking space
    text.remove(QChar(0x202F));  // narrow non-breaking space
    text.remove(QLatin1Char('\''));

    const qsizetype lastComma = text.lastIndexOf(QLatin1Char(','));
    const qsizetype lastDot = text.lastIndexOf(QLatin1Char('.'));
    if (lastComma >= 0 && lastDot >= 0) {
        // The rightmost of the two is the decimal separator; the other groups.
        if (lastComma > lastDot) {
            text.remove(QLatin1Char('.'));
            text.replace(QLatin1Char(','), QLatin1Char('.'));
        } else {
            text.remove(QLatin1Char(','));
        }
    } else if (lastComma >= 0) {
        // A lone comma is a decimal separator unless it groups thousands
        // exactly, e.g. "1,234" -- which we cannot disambiguate, so treat a
        // 3-digit tail as grouping and anything else as decimals.
        const bool looksLikeGrouping = (text.size() - lastComma - 1) == 3;
        if (looksLikeGrouping)
            text.remove(QLatin1Char(','));
        else
            text.replace(QLatin1Char(','), QLatin1Char('.'));
    }

    bool parsed = false;
    const double value = text.toDouble(&parsed);
    if (ok)
        *ok = parsed;
    if (!parsed)
        return 0;

    return static_cast<qint64>(std::llround(value * 100.0));
}

inline QString format(qint64 minor, const QString &currency)
{
    // Deliberately not cached: the default locale changes with the interface
    // language, and a static would pin the separators to whichever came first.
    const QLocale locale;
    const QString number = locale.toString(static_cast<double>(minor) / 100.0, 'f', 2);
    return currency.isEmpty() ? number : number + QLatin1Char(' ') + currency;
}

} // namespace Money
