#pragma once

#include "core/Transaction.h"
#include "providers/CsvParser.h"

/*!
 * Trading 212 history export.
 *
 * Trading 212 is not a bank and exposes no PSD2 interface, so it cannot be
 * reached through the aggregation API the banks use. Its own public API does
 * have a transactions endpoint, but that one reports card spending without the
 * merchant -- only an opaque UUID -- which is useless for budgeting. The CSV
 * the export endpoint produces does carry "Merchant name" and "Merchant
 * category", so that is what both the live provider and a manual import parse.
 *
 * The file mixes budget events with portfolio activity. Share trades move money
 * between cash and holdings rather than in or out of the user's pocket, so they
 * are dropped; deposits and withdrawals are transfers, since the other leg
 * already appears on the funding account.
 */
namespace Trading212 {

//! True if this header looks like a Trading 212 history export.
bool matches(const QStringList &header);

/*!
 * Maps an export onto normalised rows.
 *
 * Returns an empty list and sets \a errorMessage when the required columns are
 * missing. Rows that are portfolio activity rather than spending are skipped
 * silently -- that is the intended behaviour, not a failure.
 */
TransactionList parse(const Csv::Table &table, QString *errorMessage);

} // namespace Trading212
