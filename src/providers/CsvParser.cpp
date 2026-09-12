#include "providers/CsvParser.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QHash>
#include <QStringDecoder>

namespace Csv {

QString decode(const QByteArray &data)
{
    QStringDecoder utf8(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
    const QString decoded = utf8.decode(data);
    if (!utf8.hasError())
        return decoded;

    // Older Nordea exports are Windows-1252/Latin-1; those never fail to decode.
    QStringDecoder latin1(QStringDecoder::Latin1);
    return latin1.decode(data);
}

QChar detectDelimiter(const QString &text)
{
    static const QList<QChar> candidates = {QLatin1Char(';'), QLatin1Char('\t'), QLatin1Char(',')};

    QHash<QChar, int> counts;
    bool inQuotes = false;
    for (const QChar character : text) {
        if (character == QLatin1Char('"')) {
            inQuotes = !inQuotes;
            continue;
        }
        if (inQuotes)
            continue;
        if (character == QLatin1Char('\n') || character == QLatin1Char('\r'))
            break;
        if (candidates.contains(character))
            counts[character] += 1;
    }

    QChar best = QLatin1Char(',');
    int bestCount = 0;
    for (const QChar candidate : candidates) {
        if (counts.value(candidate) > bestCount) {
            bestCount = counts.value(candidate);
            best = candidate;
        }
    }
    return best;
}

Table parse(const QString &text, QChar delimiter)
{
    Table table;
    if (text.isEmpty())
        return table;

    if (delimiter.isNull())
        delimiter = detectDelimiter(text);
    table.delimiter = delimiter;

    QList<QStringList> records;
    QStringList current;
    QString field;
    bool inQuotes = false;

    const auto endField = [&] {
        current.append(field.trimmed());
        field.clear();
    };
    const auto endRecord = [&] {
        endField();
        // Skip blank trailing lines rather than emitting an empty record.
        const bool blank = current.size() == 1 && current.first().isEmpty();
        if (!blank)
            records.append(current);
        current.clear();
    };

    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar character = text.at(i);

        if (inQuotes) {
            if (character == QLatin1Char('"')) {
                const bool escapedQuote = (i + 1 < text.size())
                                          && text.at(i + 1) == QLatin1Char('"');
                if (escapedQuote) {
                    field.append(QLatin1Char('"'));
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                field.append(character);
            }
            continue;
        }

        if (character == QLatin1Char('"')) {
            inQuotes = true;
        } else if (character == delimiter) {
            endField();
        } else if (character == QLatin1Char('\n')) {
            endRecord();
        } else if (character == QLatin1Char('\r')) {
            // Handled by the following '\n'; a lone CR also terminates a record.
            if (i + 1 >= text.size() || text.at(i + 1) != QLatin1Char('\n'))
                endRecord();
        } else {
            field.append(character);
        }
    }

    if (!field.isEmpty() || !current.isEmpty())
        endRecord();

    if (records.isEmpty())
        return table;

    table.header = records.takeFirst();
    if (!table.header.isEmpty()) {
        // Strip a UTF-8 BOM that survived decoding into the first header cell.
        QString &first = table.header.first();
        if (first.startsWith(QChar(0xFEFF)))
            first.remove(0, 1);
    }
    table.rows = records;
    return table;
}

int columnIndexAllowingQualifier(const QStringList &header,
                                 const QStringList &aliases,
                                 QString *qualifier)
{
    const int exact = columnIndex(header, aliases);
    if (exact >= 0)
        return exact;

    static const QRegularExpression bracketed(QStringLiteral("^(.*?)\\s*\\(([^)]*)\\)\\s*$"));
    for (qsizetype i = 0; i < header.size(); ++i) {
        const QRegularExpressionMatch match = bracketed.match(header.at(i).trimmed());
        if (!match.hasMatch())
            continue;
        if (columnIndex({match.captured(1)}, aliases) < 0)
            continue;
        if (qualifier)
            *qualifier = match.captured(2).trimmed();
        return static_cast<int>(i);
    }
    return -1;
}

QString cell(const QStringList &row, int index)
{
    return (index >= 0 && index < row.size()) ? row.at(index).trimmed() : QString();
}

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

int columnIndex(const QStringList &header, const QStringList &aliases)
{
    const auto normalise = [](QString value) {
        value.remove(QChar(0xFEFF));
        return value.trimmed().toCaseFolded();
    };

    QStringList normalisedAliases;
    normalisedAliases.reserve(aliases.size());
    for (const QString &alias : aliases)
        normalisedAliases.append(normalise(alias));

    for (qsizetype i = 0; i < header.size(); ++i) {
        if (normalisedAliases.contains(normalise(header.at(i))))
            return static_cast<int>(i);
    }
    return -1;
}

} // namespace Csv
