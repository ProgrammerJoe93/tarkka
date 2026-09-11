#include "providers/CsvProvider.h"

#include "core/Money.h"
#include "providers/CsvParser.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>

namespace {

// Column aliases seen across Nordea (Finnish and English exports, semicolon and
// tab separated) and Revolut statement downloads.
const QStringList kBookingDateAliases = {
    QStringLiteral("Kirjauspäivä"),   QStringLiteral("Kirjauspaiva"),
    QStringLiteral("Booking date"),   QStringLiteral("Bokföringsdag"),
    QStringLiteral("Completed Date"), QStringLiteral("Date completed (UTC)"),
    QStringLiteral("Date"),           QStringLiteral("Transaction date"),
};

const QStringList kValueDateAliases = {
    QStringLiteral("Arvopäivä"),    QStringLiteral("Arvopaiva"),
    QStringLiteral("Value date"),   QStringLiteral("Valutadag"),
    QStringLiteral("Started Date"), QStringLiteral("Date started (UTC)"),
};

const QStringList kAmountAliases = {
    QStringLiteral("Määrä"),  QStringLiteral("Maara"),  QStringLiteral("Amount"),
    QStringLiteral("Belopp"), QStringLiteral("Summa"),
};

const QStringList kCurrencyAliases = {
    QStringLiteral("Valuutta"), QStringLiteral("Currency"), QStringLiteral("Valuta"),
};

const QStringList kDescriptionAliases = {
    QStringLiteral("Otsikko"),     QStringLiteral("Viesti"),
    QStringLiteral("Description"), QStringLiteral("Message"),
    QStringLiteral("Title"),       QStringLiteral("Beskrivning"),
    QStringLiteral("Tapahtumalaji"),
};

const QStringList kCounterpartyAliases = {
    QStringLiteral("Saaja/Maksaja"), QStringLiteral("Nimi"),
    QStringLiteral("Saaja"),         QStringLiteral("Maksaja"),
    QStringLiteral("Recipient"),     QStringLiteral("Payee"),
    QStringLiteral("Sender"),        QStringLiteral("Name"),
    QStringLiteral("Betalningsmottagare"),
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

QString cell(const QStringList &row, int index)
{
    return (index >= 0 && index < row.size()) ? row.at(index).trimmed() : QString();
}

/*!
 * Parses the date formats these exports actually use.
 *
 * Revolut writes ISO timestamps, Nordea writes dd.MM.yyyy, and English exports
 * sometimes use yyyy-MM-dd. Anything unrecognised returns an invalid date so
 * the row can be reported rather than silently dated today.
 */
QDate parseDate(const QString &text)
{
    if (text.isEmpty())
        return {};

    static const QStringList formats = {
        QStringLiteral("yyyy-MM-dd"),   QStringLiteral("dd.MM.yyyy"),
        QStringLiteral("d.M.yyyy"),     QStringLiteral("dd/MM/yyyy"),
        QStringLiteral("MM/dd/yyyy"),   QStringLiteral("yyyy/MM/dd"),
        QStringLiteral("dd-MM-yyyy"),
    };

    // "2026-09-12 14:03:11" and "2026-09-12T14:03:11Z" both start with a date.
    const QString datePart = text.left(10);
    for (const QString &format : formats) {
        const QDate date = QDate::fromString(datePart, format);
        if (date.isValid())
            return date;
    }
    for (const QString &format : formats) {
        const QDate date = QDate::fromString(text, format);
        if (date.isValid())
            return date;
    }

    const QDateTime timestamp = QDateTime::fromString(text, Qt::ISODate);
    return timestamp.isValid() ? timestamp.date() : QDate();
}

} // namespace

CsvProvider::CsvProvider(QObject *parent)
    : Provider(parent)
{
}

QString CsvProvider::detectInstitution(const QStringList &header)
{
    // Revolut is the only one of the two that ships a "Product" column next to
    // "Started Date"; Nordea exports carry Finnish or Swedish column names.
    if (Csv::columnIndex(header, {QStringLiteral("Product")}) >= 0
        && Csv::columnIndex(header, {QStringLiteral("Started Date")}) >= 0) {
        return QStringLiteral("Revolut");
    }
    for (const QString &marker : {QStringLiteral("Kirjauspäivä"),
                                  QStringLiteral("Arkistointitunnus"),
                                  QStringLiteral("Saaja/Maksaja")}) {
        if (Csv::columnIndex(header, {marker}) >= 0)
            return QStringLiteral("Nordea");
    }
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

    const int dateColumn = Csv::columnIndex(table.header, kBookingDateAliases);
    const int amountColumn = Csv::columnIndex(table.header, kAmountAliases);
    if (dateColumn < 0 || amountColumn < 0) {
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
        const QDate bookingDate = parseDate(cell(row, dateColumn));
        bool amountOk = false;
        const qint64 amountMinor = Money::fromDecimalString(cell(row, amountColumn), &amountOk);

        if (!bookingDate.isValid() || !amountOk) {
            ++skipped;
            continue;
        }

        Transaction transaction;
        transaction.source = sourceId();
        transaction.institution = institution.isEmpty() ? QStringLiteral("Imported") : institution;
        transaction.accountName = cell(row, productColumn);
        transaction.accountId = transaction.institution
                                + (transaction.accountName.isEmpty()
                                       ? QString()
                                       : QLatin1Char('/') + transaction.accountName);
        transaction.bookingDate = bookingDate;
        transaction.valueDate = valueDateColumn >= 0 ? parseDate(cell(row, valueDateColumn))
                                                     : bookingDate;
        transaction.counterparty = cell(row, counterpartyColumn);
        transaction.description = cell(row, descriptionColumn);
        if (transaction.description.isEmpty())
            transaction.description = transaction.counterparty;
        transaction.reference = cell(row, referenceColumn);
        transaction.currency = currencyColumn >= 0 ? cell(row, currencyColumn)
                                                   : QStringLiteral("EUR");
        transaction.status = statusColumn >= 0 ? cell(row, statusColumn) : QStringLiteral("BOOK");

        // Revolut reports the card fee in its own column; fold it into the row
        // so the total matches what actually left the account.
        transaction.amountMinor = amountMinor;
        if (feeColumn >= 0) {
            bool feeOk = false;
            const qint64 fee = Money::fromDecimalString(cell(row, feeColumn), &feeOk);
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

void CsvProvider::importFile(const QString &filePath, const QString &institutionHint)
{
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
