#include "data/TransactionStore.h"

#include "core/Categoriser.h"

#include <QDir>
#include <QFileInfo>
#include <QPair>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>

namespace {

//! Whether the transactions table already carries \a column.
bool hasColumn(const QSqlDatabase &database, const QString &column)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("PRAGMA table_info(transactions)")))
        return false;
    while (query.next()) {
        if (query.value(1).toString().compare(column, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

Transaction rowToTransaction(const QSqlQuery &query)
{
    Transaction transaction;
    transaction.id = query.value(QStringLiteral("id")).toString();
    transaction.source = query.value(QStringLiteral("source")).toString();
    transaction.institution = query.value(QStringLiteral("institution")).toString();
    transaction.accountId = query.value(QStringLiteral("account_id")).toString();
    transaction.accountName = query.value(QStringLiteral("account_name")).toString();
    transaction.bookingDate = QDate::fromString(
        query.value(QStringLiteral("booking_date")).toString(), Qt::ISODate);
    transaction.valueDate = QDate::fromString(
        query.value(QStringLiteral("value_date")).toString(), Qt::ISODate);
    transaction.description = query.value(QStringLiteral("description")).toString();
    transaction.counterparty = query.value(QStringLiteral("counterparty")).toString();
    transaction.reference = query.value(QStringLiteral("reference")).toString();
    transaction.amountMinor = query.value(QStringLiteral("amount_minor")).toLongLong();
    transaction.currency = query.value(QStringLiteral("currency")).toString();
    transaction.category = query.value(QStringLiteral("category")).toString();
    transaction.status = query.value(QStringLiteral("status")).toString();
    transaction.excluded = query.value(QStringLiteral("excluded")).toInt() != 0;
    transaction.categoryFromProvider =
        query.value(QStringLiteral("category_is_provider_set")).toInt() != 0;
    return transaction;
}

} // namespace

TransactionStore::TransactionStore()
    : m_connectionName(QStringLiteral("budget-%1").arg(
          QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
}

TransactionStore::~TransactionStore()
{
    if (QSqlDatabase::contains(m_connectionName)) {
        {
            QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
            if (database.isOpen())
                database.close();
        }
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool TransactionStore::open(const QString &path, QString *errorMessage)
{
    QDir().mkpath(QFileInfo(path).absolutePath());

    QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    database.setDatabaseName(path);
    if (!database.open()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Cannot open %1: %2")
                                .arg(path, database.lastError().text());
        return false;
    }

    QSqlQuery pragma(database);
    pragma.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    pragma.exec(QStringLiteral("PRAGMA foreign_keys=ON"));

    if (!migrate(errorMessage))
        return false;

    m_open = true;
    return true;
}

bool TransactionStore::migrate(QString *errorMessage)
{
    QSqlDatabase database = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(database);

    const QStringList statements = {
        QStringLiteral("CREATE TABLE IF NOT EXISTS transactions ("
                       "  id TEXT PRIMARY KEY,"
                       "  source TEXT NOT NULL,"
                       "  institution TEXT NOT NULL,"
                       "  account_id TEXT NOT NULL,"
                       "  account_name TEXT,"
                       "  booking_date TEXT NOT NULL,"
                       "  value_date TEXT,"
                       "  description TEXT,"
                       "  counterparty TEXT,"
                       "  reference TEXT,"
                       "  amount_minor INTEGER NOT NULL,"
                       "  currency TEXT NOT NULL,"
                       "  category TEXT,"
                       "  category_is_user_set INTEGER NOT NULL DEFAULT 0,"
                       "  status TEXT,"
                       "  excluded INTEGER NOT NULL DEFAULT 0,"
                       "  category_is_provider_set INTEGER NOT NULL DEFAULT 0)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_tx_booking_date"
                       "  ON transactions(booking_date)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_tx_institution"
                       "  ON transactions(institution)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_tx_category ON transactions(category)"),
    };

    for (const QString &statement : statements) {
        if (!query.exec(statement)) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Schema migration failed: %1")
                                    .arg(query.lastError().text());
            return false;
        }
    }

    // CREATE TABLE IF NOT EXISTS does nothing to a database written by an
    // older build, so a column added after the fact has to be applied
    // separately. Asked for rather than attempted-and-ignored: swallowing the
    // error would also swallow a genuinely broken database.
    const QList<QPair<QString, QString>> addedColumns = {
        {QStringLiteral("excluded"),
         QStringLiteral("ALTER TABLE transactions"
                        " ADD COLUMN excluded INTEGER NOT NULL DEFAULT 0")},
        {QStringLiteral("category_is_provider_set"),
         QStringLiteral("ALTER TABLE transactions"
                        " ADD COLUMN category_is_provider_set INTEGER NOT NULL DEFAULT 0")},
    };
    for (const auto &[column, statement] : addedColumns) {
        if (hasColumn(database, column))
            continue;
        if (!query.exec(statement)) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Schema migration failed: %1")
                                    .arg(query.lastError().text());
            return false;
        }
    }
    return true;
}

int TransactionStore::upsert(const TransactionList &transactions, QString *errorMessage)
{
    if (transactions.isEmpty())
        return 0;

    QSqlDatabase database = QSqlDatabase::database(m_connectionName);
    if (!database.transaction()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Cannot start a database transaction: %1")
                                .arg(database.lastError().text());
        return -1;
    }

    QSqlQuery existing(database);
    existing.prepare(QStringLiteral("SELECT 1 FROM transactions WHERE id = ?"));

    QSqlQuery insert(database);
    // An amended row overwrites the bank-sourced columns, but a category the
    // user set by hand outranks whatever the categoriser guesses on re-sync.
    // The exclusion flag is user state too, so the UPDATE clause leaves it
    // alone rather than resetting a row the user had already set aside.
    insert.prepare(QStringLiteral(
        "INSERT INTO transactions (id, source, institution, account_id, account_name,"
        "  booking_date, value_date, description, counterparty, reference,"
        "  amount_minor, currency, category, category_is_user_set, status, excluded,"
        "  category_is_provider_set)"
        " VALUES (:id, :source, :institution, :account_id, :account_name,"
        "  :booking_date, :value_date, :description, :counterparty, :reference,"
        "  :amount_minor, :currency, :category, 0, :status, 0, :from_provider)"
        " ON CONFLICT(id) DO UPDATE SET"
        "  account_name = excluded.account_name,"
        "  booking_date = excluded.booking_date,"
        "  value_date = excluded.value_date,"
        "  description = excluded.description,"
        "  counterparty = excluded.counterparty,"
        "  reference = excluded.reference,"
        "  amount_minor = excluded.amount_minor,"
        "  currency = excluded.currency,"
        "  status = excluded.status,"
        "  category = CASE WHEN transactions.category_is_user_set = 1"
        "                  THEN transactions.category ELSE excluded.category END,"
        "  category_is_provider_set = excluded.category_is_provider_set"));

    int inserted = 0;
    for (const Transaction &transaction : transactions) {
        existing.addBindValue(transaction.id);
        const bool isNew = !(existing.exec() && existing.next());
        existing.finish();

        insert.bindValue(QStringLiteral(":id"), transaction.id);
        insert.bindValue(QStringLiteral(":source"), transaction.source);
        insert.bindValue(QStringLiteral(":institution"), transaction.institution);
        insert.bindValue(QStringLiteral(":account_id"), transaction.accountId);
        insert.bindValue(QStringLiteral(":account_name"), transaction.accountName);
        insert.bindValue(QStringLiteral(":booking_date"),
                         transaction.bookingDate.toString(Qt::ISODate));
        insert.bindValue(QStringLiteral(":value_date"),
                         transaction.valueDate.toString(Qt::ISODate));
        insert.bindValue(QStringLiteral(":description"), transaction.description);
        insert.bindValue(QStringLiteral(":counterparty"), transaction.counterparty);
        insert.bindValue(QStringLiteral(":reference"), transaction.reference);
        insert.bindValue(QStringLiteral(":amount_minor"), transaction.amountMinor);
        insert.bindValue(QStringLiteral(":currency"), transaction.currency);
        insert.bindValue(QStringLiteral(":category"), transaction.category);
        insert.bindValue(QStringLiteral(":status"), transaction.status);
        insert.bindValue(QStringLiteral(":from_provider"), transaction.categoryFromProvider ? 1 : 0);

        if (!insert.exec()) {
            const QString failure = insert.lastError().text();
            database.rollback();
            if (errorMessage)
                *errorMessage = QStringLiteral("Could not save a transaction: %1").arg(failure);
            return -1;
        }
        if (isNew)
            ++inserted;
    }

    if (!database.commit()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Could not commit the sync: %1")
                                .arg(database.lastError().text());
        database.rollback();
        return -1;
    }
    return inserted;
}

TransactionList TransactionStore::query(const Filter &filter, QString *errorMessage) const
{
    QSqlDatabase database = QSqlDatabase::database(m_connectionName);
    QString sql = QStringLiteral("SELECT * FROM transactions WHERE 1=1");
    QVariantList bindings;

    if (filter.from.isValid()) {
        sql += QStringLiteral(" AND booking_date >= ?");
        bindings << filter.from.toString(Qt::ISODate);
    }
    if (filter.to.isValid()) {
        sql += QStringLiteral(" AND booking_date <= ?");
        bindings << filter.to.toString(Qt::ISODate);
    }
    if (!filter.institution.isEmpty()) {
        sql += QStringLiteral(" AND institution = ?");
        bindings << filter.institution;
    }
    if (!filter.category.isEmpty()) {
        sql += QStringLiteral(" AND category = ?");
        bindings << filter.category;
    }
    if (!filter.currency.isEmpty()) {
        sql += QStringLiteral(" AND currency = ?");
        bindings << filter.currency;
    }
    if (filter.spendOnly)
        sql += QStringLiteral(" AND amount_minor < 0");
    if (!filter.searchText.isEmpty()) {
        sql += QStringLiteral(" AND (description LIKE ? OR counterparty LIKE ?"
                              " OR reference LIKE ?)");
        const QString pattern = QLatin1Char('%') + filter.searchText + QLatin1Char('%');
        bindings << pattern << pattern << pattern;
    }
    sql += QStringLiteral(" ORDER BY booking_date DESC, id DESC");

    QSqlQuery query(database);
    query.prepare(sql);
    for (const QVariant &binding : std::as_const(bindings))
        query.addBindValue(binding);

    if (!query.exec()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Query failed: %1").arg(query.lastError().text());
        return {};
    }

    TransactionList result;
    while (query.next())
        result.append(rowToTransaction(query));
    return result;
}

QList<TransactionStore::MonthlySpend> TransactionStore::monthlySpend(const QDate &from,
                                                                    const QDate &to,
                                                                    const QString &currency,
                                                                    QString *errorMessage) const
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    // substr on an ISO date is the month key; SQLite has no date type of its
    // own, and the column is stored ISO precisely so that this kind of prefix
    // comparison works.
    query.prepare(QStringLiteral(
        "SELECT substr(booking_date, 1, 7) AS ym, category, SUM(-amount_minor) AS spent"
        " FROM transactions"
        " WHERE amount_minor < 0"
        "   AND excluded = 0"
        "   AND currency = ?"
        "   AND booking_date >= ? AND booking_date <= ?"
        "   AND category <> ? COLLATE NOCASE"
        " GROUP BY ym, category"
        " ORDER BY ym, spent DESC"));
    query.addBindValue(currency);
    query.addBindValue(from.toString(Qt::ISODate));
    query.addBindValue(to.toString(Qt::ISODate));
    query.addBindValue(Categoriser::transfers());

    if (!query.exec()) {
        if (errorMessage)
            *errorMessage = query.lastError().text();
        return {};
    }

    QList<MonthlySpend> results;
    while (query.next()) {
        MonthlySpend entry;
        // "2026-09" names the month; the first of it stands for the whole.
        entry.month = QDate::fromString(query.value(0).toString() + QStringLiteral("-01"),
                                        Qt::ISODate);
        entry.category = query.value(1).toString();
        entry.spentMinor = query.value(2).toLongLong();
        if (entry.month.isValid())
            results.append(entry);
    }
    return results;
}

QString TransactionStore::dominantCurrency(const QString &institution) const
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral("SELECT currency FROM transactions"
                                 " WHERE institution = ? AND currency <> ''"
                                 " GROUP BY currency ORDER BY COUNT(*) DESC, currency LIMIT 1"));
    query.addBindValue(institution);
    if (query.exec() && query.next())
        return query.value(0).toString();
    return {};
}

int TransactionStore::countInCategory(const QString &category) const
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM transactions WHERE category = ?"));
    query.addBindValue(category);
    if (query.exec() && query.next())
        return query.value(0).toInt();
    return 0;
}

QStringList TransactionStore::institutions() const
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    QStringList values;
    if (query.exec(QStringLiteral("SELECT DISTINCT institution FROM transactions"
                                  " ORDER BY institution"))) {
        while (query.next())
            values.append(query.value(0).toString());
    }
    return values;
}

QStringList TransactionStore::categories() const
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    QStringList values;
    if (query.exec(QStringLiteral("SELECT DISTINCT category FROM transactions"
                                  " WHERE category <> '' ORDER BY category"))) {
        while (query.next())
            values.append(query.value(0).toString());
    }
    return values;
}

QStringList TransactionStore::currencies() const
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    QStringList values;
    if (query.exec(QStringLiteral("SELECT currency FROM transactions WHERE currency <> ''"
                                  " GROUP BY currency ORDER BY COUNT(*) DESC, currency"))) {
        while (query.next())
            values.append(query.value(0).toString());
    }
    return values;
}

QDate TransactionStore::earliestDate() const
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    if (query.exec(QStringLiteral("SELECT MIN(booking_date) FROM transactions")) && query.next())
        return QDate::fromString(query.value(0).toString(), Qt::ISODate);
    return {};
}

bool TransactionStore::setCategory(const QString &transactionId, const QString &category,
                                   QString *errorMessage)
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral("UPDATE transactions SET category = ?,"
                                 " category_is_user_set = 1 WHERE id = ?"));
    query.addBindValue(category);
    query.addBindValue(transactionId);
    if (!query.exec()) {
        if (errorMessage)
            *errorMessage = query.lastError().text();
        return false;
    }
    return true;
}

int TransactionStore::recategorise(const Categoriser &categoriser, QString *errorMessage)
{
    QSqlDatabase database = QSqlDatabase::database(m_connectionName);
    if (!database.transaction()) {
        if (errorMessage)
            *errorMessage = database.lastError().text();
        return -1;
    }

    QSqlQuery read(database);
    // Neither the user's own decisions nor a provider's: only rows whose
    // category came from the rules in the first place are the rules' to revise.
    if (!read.exec(QStringLiteral("SELECT * FROM transactions"
                                  " WHERE category_is_user_set = 0"
                                  "   AND category_is_provider_set = 0"))) {
        if (errorMessage)
            *errorMessage = read.lastError().text();
        database.rollback();
        return -1;
    }

    TransactionList rows;
    while (read.next())
        rows.append(rowToTransaction(read));
    read.finish();

    QSqlQuery update(database);
    update.prepare(QStringLiteral("UPDATE transactions SET category = ? WHERE id = ?"));

    int changed = 0;
    for (const Transaction &transaction : rows) {
        const QString category = categoriser.categorise(transaction);
        if (category == transaction.category)
            continue;

        update.addBindValue(category);
        update.addBindValue(transaction.id);
        if (!update.exec()) {
            if (errorMessage)
                *errorMessage = update.lastError().text();
            database.rollback();
            return -1;
        }
        update.finish();
        ++changed;
    }

    if (!database.commit()) {
        if (errorMessage)
            *errorMessage = database.lastError().text();
        database.rollback();
        return -1;
    }
    return changed;
}

bool TransactionStore::setExcluded(const QString &transactionId, bool excluded,
                                  QString *errorMessage)
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral("UPDATE transactions SET excluded = ? WHERE id = ?"));
    query.addBindValue(excluded ? 1 : 0);
    query.addBindValue(transactionId);
    if (!query.exec()) {
        if (errorMessage)
            *errorMessage = query.lastError().text();
        return false;
    }
    return true;
}

bool TransactionStore::clear(QString *errorMessage)
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    if (!query.exec(QStringLiteral("DELETE FROM transactions"))) {
        if (errorMessage)
            *errorMessage = query.lastError().text();
        return false;
    }
    return true;
}

bool TransactionStore::removeInstitution(const QString &institution, QString *errorMessage)
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral("DELETE FROM transactions WHERE institution = ?"));
    query.addBindValue(institution);
    if (!query.exec()) {
        if (errorMessage)
            *errorMessage = query.lastError().text();
        return false;
    }
    return true;
}
