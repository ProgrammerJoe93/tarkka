#pragma once

#include <QByteArray>
#include <QDate>
#include <QString>
#include <QStringList>

/*!
 * A small RFC 4180 style CSV reader.
 *
 * Bank exports vary in delimiter (Nordea uses semicolons or tabs, Revolut uses
 * commas) and in encoding, so both are detected rather than assumed. Quoted
 * fields may contain the delimiter, doubled quotes and newlines.
 */
namespace Csv {

struct Table
{
    QStringList header;
    QList<QStringList> rows;
    QChar delimiter;
    bool isEmpty() const { return header.isEmpty() && rows.isEmpty(); }
};

//! Guesses the delimiter by counting candidates outside quotes on the first line.
QChar detectDelimiter(const QString &text);

//! Decodes \a data as UTF-8, falling back to Latin-1 when that is not valid UTF-8.
QString decode(const QByteArray &data);

Table parse(const QString &text, QChar delimiter = QChar());

/*!
 * Finds the index of the first column whose header matches one of \a aliases.
 *
 * Matching ignores case, surrounding whitespace and BOM noise, so the same
 * alias table copes with "Amount", "amount" and "﻿Amount".
 */
int columnIndex(const QStringList &header, const QStringList &aliases);

/*!
 * Like columnIndex, but also matches a header that carries a parenthetical
 * qualifier, so "Amount (GBP)" matches the alias "Amount".
 *
 * When it does, the text inside the brackets is written to \a qualifier --
 * useful because several UK banks name the currency there and nowhere else.
 */
int columnIndexAllowingQualifier(const QStringList &header,
                                 const QStringList &aliases,
                                 QString *qualifier = nullptr);

//! Returns the cell at \a index, trimmed, or an empty string if out of range.
QString cell(const QStringList &row, int index);

/*!
 * Parses the date formats bank and broker exports actually use.
 *
 * Revolut and Trading 212 write ISO timestamps, Nordea writes dd.MM.yyyy, and
 * English exports sometimes use yyyy-MM-dd. Anything unrecognised returns an
 * invalid date so the row can be reported rather than silently dated today.
 */
QDate parseDate(const QString &text);

} // namespace Csv
