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
    void importFile(const QString &filePath, const QString &institutionHint = {});

    //! Parses \a data directly. Exposed for tests and for clipboard imports.
    TransactionList parse(const QByteArray &data,
                          const QString &institutionHint,
                          QString *errorMessage) const;

    //! Best guess at which bank produced a file with this header row.
    static QString detectInstitution(const QStringList &header);
};
