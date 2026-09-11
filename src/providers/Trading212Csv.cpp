#include "providers/Trading212Csv.h"

#include "core/Categoriser.h"
#include "core/Money.h"

namespace {

const QStringList kActionAliases = {QStringLiteral("Action"), QStringLiteral("Type")};
const QStringList kTimeAliases = {QStringLiteral("Time"), QStringLiteral("Date")};
const QStringList kTotalAliases = {QStringLiteral("Total"), QStringLiteral("Amount")};
const QStringList kCurrencyAliases = {QStringLiteral("Currency (Total)"),
                                      QStringLiteral("Currency")};
const QStringList kMerchantAliases = {QStringLiteral("Merchant name")};
const QStringList kMerchantCategoryAliases = {QStringLiteral("Merchant category")};
const QStringList kNotesAliases = {QStringLiteral("Notes")};
const QStringList kIdAliases = {QStringLiteral("ID")};
const QStringList kInstrumentAliases = {QStringLiteral("Name"), QStringLiteral("Ticker")};

/*!
 * How one Action value should be treated.
 *
 * \c include is false for portfolio activity. \c sign forces the direction,
 * because the export's own sign convention is not consistent across action
 * types; 0 means "trust the sign in the file".
 */
struct ActionRule
{
    const char *needle;
    bool include;
    int sign;
    const char *category;  //!< Empty means let the merchant categoriser decide.
};

// Matched as case-insensitive substrings, most specific first.
constexpr ActionRule kActionRules[] = {
    {"card debit", true, -1, ""},
    {"card credit", true, +1, "Refunds"},
    {"card refund", true, +1, "Refunds"},
    {"cashback", true, +1, "Cashback"},
    {"card", true, 0, ""},
    // The funding leg already appears on the bank account it came from.
    {"deposit", true, +1, "Transfers"},
    {"withdrawal", true, -1, "Transfers"},
    {"interest", true, +1, "Income"},
    {"dividend", true, +1, "Income"},
    // Portfolio activity: cash moving into or out of holdings, not spending.
    {"market buy", false, 0, ""},
    {"market sell", false, 0, ""},
    {"limit buy", false, 0, ""},
    {"limit sell", false, 0, ""},
    {"stop buy", false, 0, ""},
    {"stop sell", false, 0, ""},
    {"autoinvest", false, 0, ""},
    {"pie", false, 0, ""},
    {"currency conversion", false, 0, ""},
};

const ActionRule *ruleFor(const QString &action)
{
    for (const ActionRule &rule : kActionRules) {
        if (action.contains(QLatin1String(rule.needle), Qt::CaseInsensitive))
            return &rule;
    }
    return nullptr;
}

} // namespace

namespace Trading212 {

bool matches(const QStringList &header)
{
    if (Csv::columnIndex(header, kActionAliases) < 0)
        return false;

    // "Action" alone is too generic; pair it with something only this export has.
    return Csv::columnIndex(header, kMerchantAliases) >= 0
           || Csv::columnIndex(header, {QStringLiteral("ISIN")}) >= 0
           || Csv::columnIndex(header, {QStringLiteral("No. of shares")}) >= 0;
}

TransactionList parse(const Csv::Table &table, QString *errorMessage)
{
    const int actionColumn = Csv::columnIndex(table.header, kActionAliases);
    const int timeColumn = Csv::columnIndex(table.header, kTimeAliases);
    const int totalColumn = Csv::columnIndex(table.header, kTotalAliases);

    if (actionColumn < 0 || timeColumn < 0 || totalColumn < 0) {
        if (errorMessage) {
            *errorMessage =
                QStringLiteral("This looks like a Trading 212 export but is missing an "
                               "Action, Time or Total column. The header was: %1")
                    .arg(table.header.join(QLatin1String(" | ")));
        }
        return {};
    }

    const int currencyColumn = Csv::columnIndex(table.header, kCurrencyAliases);
    const int merchantColumn = Csv::columnIndex(table.header, kMerchantAliases);
    const int merchantCategoryColumn = Csv::columnIndex(table.header, kMerchantCategoryAliases);
    const int notesColumn = Csv::columnIndex(table.header, kNotesAliases);
    const int idColumn = Csv::columnIndex(table.header, kIdAliases);
    const int instrumentColumn = Csv::columnIndex(table.header, kInstrumentAliases);

    TransactionList transactions;
    transactions.reserve(table.rows.size());
    int ordinal = 0;
    int skippedUnreadable = 0;
    int consideredBudgetRows = 0;

    for (const QStringList &row : table.rows) {
        const QString action = Csv::cell(row, actionColumn);
        const ActionRule *rule = ruleFor(action);
        if (rule && !rule->include)
            continue;

        ++consideredBudgetRows;

        const QDate date = Csv::parseDate(Csv::cell(row, timeColumn));
        bool amountOk = false;
        const qint64 total = Money::fromDecimalString(Csv::cell(row, totalColumn), &amountOk);
        if (!date.isValid() || !amountOk) {
            ++skippedUnreadable;
            continue;
        }

        Transaction transaction;
        transaction.source = QStringLiteral("csv");
        transaction.institution = QStringLiteral("Trading 212");
        transaction.accountId = QStringLiteral("Trading 212/card");
        transaction.accountName = QStringLiteral("212 Card");
        transaction.bookingDate = date;
        transaction.valueDate = date;
        transaction.status = QStringLiteral("BOOK");

        const int sign = rule ? rule->sign : 0;
        transaction.amountMinor = sign == 0 ? total : sign * qAbs(total);

        transaction.currency = currencyColumn >= 0 ? Csv::cell(row, currencyColumn) : QString();
        if (transaction.currency.isEmpty())
            transaction.currency = QStringLiteral("EUR");

        transaction.counterparty = Csv::cell(row, merchantColumn);
        if (transaction.counterparty.isEmpty())
            transaction.counterparty = Csv::cell(row, instrumentColumn);
        if (transaction.counterparty.isEmpty())
            transaction.counterparty = action;

        transaction.description = Csv::cell(row, notesColumn);
        if (transaction.description.isEmpty())
            transaction.description = action;

        transaction.reference = Csv::cell(row, idColumn);

        if (rule && *rule->category)
            transaction.category = QString::fromLatin1(rule->category);

        // The merchant category the card reports is a better starting point
        // than a name-pattern guess, so keep it when nothing else applies.
        if (transaction.category.isEmpty() && merchantCategoryColumn >= 0) {
            const QString reported = Csv::cell(row, merchantCategoryColumn);
            if (!reported.isEmpty())
                transaction.category = reported;
        }

        transaction.id = makeTransactionId(transaction.source,
                                           transaction.accountId,
                                           transaction.reference,
                                           transaction.bookingDate,
                                           transaction.amountMinor,
                                           transaction.description,
                                           ordinal++);
        transactions.append(transaction);
    }

    if (transactions.isEmpty() && errorMessage) {
        *errorMessage = consideredBudgetRows == 0
                            ? QStringLiteral("The export contained only portfolio activity, "
                                             "no card spending or cash movements.")
                            : QStringLiteral("None of the %1 spending rows had a readable "
                                             "date and total.")
                                  .arg(skippedUnreadable);
    }
    return transactions;
}

} // namespace Trading212
