#pragma once

#include "providers/Provider.h"

#include <QStringList>

/*!
 * Imports transactions from a bank's own statement export.
 *
 * This is the path that always works: it needs no credentials, no consent flow
 * and no API entitlement, which matters because a personal Revolut account has
 * no API of its own. Column layouts differ between banks and change between
 * export versions, so columns are located by header name from an alias table
 * rather than by fixed position.
 */
class CsvProvider : public Provider
{
    Q_OBJECT

public:
    explicit CsvProvider(QObject *parent = nullptr);

    QString sourceId() const override { return QStringLiteral("csv"); }
    QString displayName() const override { return QStringLiteral("Statement import"); }

    /*!
     * Parses \a filePath and emits the rows it contains.
     *
     * \a institutionHint names the bank when the file itself does not make it
     * obvious; pass an empty string to let the importer guess from the header.
     */
    void importFile(const QString &filePath,
                    const QString &institutionHint = {},
                    const QString &defaultCurrency = {});

    /*!
     * Currency to assume when the file names none.
     *
     * Several UK banks export neither a currency column nor a qualified amount
     * header. Guessing euros there would file sterling rows into the euro
     * totals, so the importer takes the answer from the caller instead.
     */
    void setDefaultCurrency(const QString &code);

    //! Parses \a data directly. Exposed for tests and for clipboard imports.
    TransactionList parse(const QByteArray &data,
                          const QString &institutionHint,
                          QString *errorMessage) const;

    //! Best guess at which bank produced a file with this header row.
    static QString detectInstitution(const QStringList &header);

private:
    QString m_defaultCurrency = QStringLiteral("EUR");
};
