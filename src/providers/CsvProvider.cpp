#include "providers/CsvProvider.h"

#include "core/Money.h"
#include "providers/CsvParser.h"
#include "providers/Trading212Csv.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>

namespace {

// Column aliases seen across Nordea (Finnish and English exports, semicolon and
// tab separated), Revolut statement downloads, and the shared Finnish
// vocabulary the other domestic banks (S-Pankki, OP, Danske, Aktia) use.
const QStringList kBookingDateAliases = {
    QStringLiteral("Kirjauspäivä"),   QStringLiteral("Kirjauspaiva"),
    QStringLiteral("Transaction Date"),
    QStringLiteral("Booking date"),   QStringLiteral("Bokföringsdag"),
    QStringLiteral("Completed Date"), QStringLiteral("Date completed (UTC)"),
    QStringLiteral("Date"),           QStringLiteral("Transaction date"),
    QStringLiteral("Maksupäivä"),     QStringLiteral("Tapahtumapäivä"),
    QStringLiteral("Kirjauspvm"),
};

const QStringList kValueDateAliases = {
    QStringLiteral("Arvopäivä"),    QStringLiteral("Arvopaiva"),
    QStringLiteral("Value date"),   QStringLiteral("Valutadag"),
    QStringLiteral("Started Date"), QStringLiteral("Date started (UTC)"),
};

const QStringList kAmountAliases = {
    QStringLiteral("Määrä"),  QStringLiteral("Maara"),  QStringLiteral("Amount"),
    QStringLiteral("Belopp"), QStringLiteral("Summa"),  QStringLiteral("Value"),
};

// Several UK banks split direction across two columns instead of signing one.
const QStringList kDebitAliases = {
    QStringLiteral("Debit Amount"), QStringLiteral("Money Out"),
    QStringLiteral("Paid Out"),     QStringLiteral("Withdrawn"),
    QStringLiteral("Debit"),
};

const QStringList kCreditAliases = {
    QStringLiteral("Credit Amount"), QStringLiteral("Money In"),
    QStringLiteral("Paid In"),       QStringLiteral("Deposited"),
    QStringLiteral("Credit"),
};

const QStringList kCurrencyAliases = {
    QStringLiteral("Valuutta"), QStringLiteral("Currency"), QStringLiteral("Valuta"),
};

const QStringList kDescriptionAliases = {
    QStringLiteral("Otsikko"),     QStringLiteral("Viesti"),
    QStringLiteral("Description"), QStringLiteral("Message"),
    QStringLiteral("Title"),       QStringLiteral("Beskrivning"),
    QStringLiteral("Tapahtumalaji"), QStringLiteral("Selite"),
    QStringLiteral("Tapahtumatyyppi"),
    QStringLiteral("Transaction Description"), QStringLiteral("Memo"),
    QStringLiteral("Narrative"), QStringLiteral("Details"),
};

const QStringList kCounterpartyAliases = {
    QStringLiteral("Saaja/Maksaja"), QStringLiteral("Nimi"),
    QStringLiteral("Saaja"),         QStringLiteral("Maksaja"),
    QStringLiteral("Recipient"),     QStringLiteral("Payee"),
    QStringLiteral("Sender"),        QStringLiteral("Name"),
    QStringLiteral("Betalningsmottagare"),
    QStringLiteral("Saajan nimi"),   QStringLiteral("Maksajan nimi"),
    QStringLiteral("Saaja tai maksaja"), QStringLiteral("Vastapuoli"),
    QStringLiteral("Counter Party"), QStringLiteral("Counterparty"),
};

const QStringList kReferenceAliases = {
    QStringLiteral("Viite"),          QStringLiteral("Viitenumero"),
    QStringLiteral("Reference"),      QStringLiteral("Reference number"),
    QStringLiteral("Arkistointitunnus"), QStringLiteral("Referens"),
};

const QStringList kStatusAliases = {
    QStringLiteral("State"), QStringLiteral("Status"),
};

const QStringList kProductAliases = {
    QStringLiteral("Product"), QStringLiteral("Tili"), QStringLiteral("Account"),
};

const QStringList kFeeAliases = {
    QStringLiteral("Fee"), QStringLiteral("Palkkio"),
};

} // namespace

CsvProvider::CsvProvider(QObject *parent)
    : Provider(parent)
{
}

QString CsvProvider::detectInstitution(const QStringList &header)
{
    // Revolut is the only one of the two that ships a "Product" column next to
    // "Started Date"; Nordea exports carry Finnish or Swedish column names.
    if (Trading212::matches(header))
        return QStringLiteral("Trading 212");

    if (Csv::columnIndex(header, {QStringLiteral("Product")}) >= 0
        && Csv::columnIndex(header, {QStringLiteral("Started Date")}) >= 0) {
        return QStringLiteral("Revolut");
    }
    for (const QString &marker : {QStringLiteral("Arkistointitunnus"),
                                  QStringLiteral("Saaja/Maksaja")}) {
        if (Csv::columnIndex(header, {marker}) >= 0)
            return QStringLiteral("Nordea");
    }

    // Other Finnish banks share too much vocabulary to tell apart from the
    // header alone, so they are left unlabelled unless the caller says which
    // bank the file came from.
    return {};
}

TransactionList CsvProvider::parse(const QByteArray &data,
                                   const QString &institutionHint,
                                   QString *errorMessage) const
{
    const Csv::Table table = Csv::parse(Csv::decode(data));
    if (table.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("The file is empty.");
        return {};
    }

    // Trading 212 mixes card spending with portfolio activity and encodes the
    // direction in an Action column, so it needs its own mapping rather than
    // the column-alias treatment the bank exports get.
    if (Trading212::matches(table.header))
        return Trading212::parse(table, errorMessage);

    const int dateColumn = Csv::columnIndex(table.header, kBookingDateAliases);

    QString amountQualifier;
    const int amountColumn =
        Csv::columnIndexAllowingQualifier(table.header, kAmountAliases, &amountQualifier);
    const int debitColumn = Csv::columnIndex(table.header, kDebitAliases);
    const int creditColumn = Csv::columnIndex(table.header, kCreditAliases);
    const bool hasSplitAmount = debitColumn >= 0 || creditColumn >= 0;

    if (dateColumn < 0 || (amountColumn < 0 && !hasSplitAmount)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                                "Could not find a date and an amount column. The header was: %1")
                                .arg(table.header.join(QLatin1String(" | ")));
        }
        return {};
    }

    const int valueDateColumn = Csv::columnIndex(table.header, kValueDateAliases);
    const int currencyColumn = Csv::columnIndex(table.header, kCurrencyAliases);
    const int descriptionColumn = Csv::columnIndex(table.header, kDescriptionAliases);
    const int counterpartyColumn = Csv::columnIndex(table.header, kCounterpartyAliases);
    const int referenceColumn = Csv::columnIndex(table.header, kReferenceAliases);
    const int statusColumn = Csv::columnIndex(table.header, kStatusAliases);
    const int productColumn = Csv::columnIndex(table.header, kProductAliases);
    const int feeColumn = Csv::columnIndex(table.header, kFeeAliases);

    const QString institution = institutionHint.isEmpty() ? detectInstitution(table.header)
                                                          : institutionHint;

    TransactionList transactions;
    transactions.reserve(table.rows.size());
    int skipped = 0;
    int ordinal = 0;

    for (const QStringList &row : table.rows) {
        const QDate bookingDate = Csv::parseDate(Csv::cell(row, dateColumn));

        bool amountOk = false;
        qint64 amountMinor = 0;
        if (amountColumn >= 0) {
            amountMinor = Money::fromDecimalString(Csv::cell(row, amountColumn), &amountOk);
        } else {
            // A split pair leaves the unused side blank, so either alone is
            // enough; money out is negative regardless of how it was written.
            bool debitOk = false;
            bool creditOk = false;
            const qint64 debit =
                Money::fromDecimalString(Csv::cell(row, debitColumn), &debitOk);
            const qint64 credit =
                Money::fromDecimalString(Csv::cell(row, creditColumn), &creditOk);
            amountOk = debitOk || creditOk;
            amountMinor = (creditOk ? qAbs(credit) : 0) - (debitOk ? qAbs(debit) : 0);
        }

        if (!bookingDate.isValid() || !amountOk) {
            ++skipped;
            continue;
        }

        Transaction transaction;
        transaction.source = sourceId();
        transaction.institution = institution.isEmpty() ? QStringLiteral("Imported") : institution;
        transaction.accountName = Csv::cell(row, productColumn);
        transaction.accountId = transaction.institution
                                + (transaction.accountName.isEmpty()
                                       ? QString()
                                       : QLatin1Char('/') + transaction.accountName);
        transaction.bookingDate = bookingDate;
        transaction.valueDate = valueDateColumn >= 0 ? Csv::parseDate(Csv::cell(row, valueDateColumn))
                                                     : bookingDate;
        transaction.counterparty = Csv::cell(row, counterpartyColumn);
        transaction.description = Csv::cell(row, descriptionColumn);
        if (transaction.description.isEmpty())
            transaction.description = transaction.counterparty;
        transaction.reference = Csv::cell(row, referenceColumn);
        transaction.currency = currencyColumn >= 0 ? Csv::cell(row, currencyColumn) : QString();
        if (transaction.currency.isEmpty())
            transaction.currency = amountQualifier;
        if (transaction.currency.isEmpty())
            transaction.currency = m_defaultCurrency;
        transaction.status = statusColumn >= 0 ? Csv::cell(row, statusColumn) : QStringLiteral("BOOK");

        // Revolut reports the card fee in its own column; fold it into the row
        // so the total matches what actually left the account.
        transaction.amountMinor = amountMinor;
        if (feeColumn >= 0) {
            bool feeOk = false;
            const qint64 fee = Money::fromDecimalString(Csv::cell(row, feeColumn), &feeOk);
            if (feeOk && fee != 0)
                transaction.amountMinor -= qAbs(fee);
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

    if (transactions.isEmpty() && skipped > 0 && errorMessage) {
        *errorMessage = QStringLiteral("None of the %1 rows had a readable date and amount.")
                            .arg(skipped);
    }
    return transactions;
}

void CsvProvider::setDefaultCurrency(const QString &code)
{
    const QString trimmed = code.trimmed().toUpper();
    if (!trimmed.isEmpty())
        m_defaultCurrency = trimmed;
}

void CsvProvider::importFile(const QString &filePath,
                             const QString &institutionHint,
                             const QString &defaultCurrency)
{
    setDefaultCurrency(defaultCurrency);

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit failed(tr("Cannot read %1: %2").arg(QFileInfo(filePath).fileName(),
                                                 file.errorString()));
        return;
    }

    QString errorMessage;
    const TransactionList transactions = parse(file.readAll(), institutionHint, &errorMessage);

    if (transactions.isEmpty()) {
        emit failed(errorMessage.isEmpty()
                        ? tr("No transactions were found in %1.").arg(QFileInfo(filePath).fileName())
                        : errorMessage);
        return;
    }

    emit transactionsReady(transactions);
    emit finished(tr("Imported %n row(s) from %1", nullptr, static_cast<int>(transactions.size()))
                      .arg(QFileInfo(filePath).fileName()));
}
