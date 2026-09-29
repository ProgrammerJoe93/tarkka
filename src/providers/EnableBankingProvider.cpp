#include "providers/EnableBankingProvider.h"

#include "core/Balance.h"
#include "core/Money.h"
#include "providers/Provider.h"

#include "auth/RedirectListener.h"
#include "core/Money.h"
#include "net/HttpClient.h"
#include "net/Jwt.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>
#include <QUuid>

namespace {

/*!
 * Something to call an account by in a message.
 *
 * Banks do not always name an account, and a failure reported as ": …" tells
 * the user nothing about which of their accounts it was.
 */
QString accountLabel(const AccountModel::Account &account)
{
    if (!account.name.isEmpty())
        return account.name;
    if (!account.identifier.isEmpty())
        return account.identifier;
    return account.institution;
}

constexpr auto kIssuer = "enablebanking.com";
constexpr auto kAudience = "api.enablebanking.com";
//! Consent lifetime requested from the bank. Banks cap this themselves.
constexpr int kConsentDays = 90;
//! JWTs are cheap to mint, so keep them short-lived.
constexpr int kTokenTtlSeconds = 3600;

QString jsonString(const QJsonObject &object, const char *key)
{
    return object.value(QLatin1String(key)).toString();
}

QDate jsonDate(const QJsonObject &object, const char *key)
{
    const QString text = jsonString(object, key);
    if (text.isEmpty())
        return {};
    const QDate date = QDate::fromString(text.left(10), Qt::ISODate);
    if (date.isValid())
        return date;
    return QDateTime::fromString(text, Qt::ISODate).date();
}

/*!
 * Reads an amount that the API may encode as a JSON string or a number.
 *
 * The Berlin Group schema specifies a decimal string, but values arriving as
 * numbers have been observed, so both are accepted rather than silently
 * yielding zero.
 */
qint64 readAmountMinor(const QJsonObject &amountObject, bool *ok)
{
    const QJsonValue value = amountObject.value(QStringLiteral("amount"));
    if (value.isString())
        return Money::fromDecimalString(value.toString(), ok);
    if (value.isDouble()) {
        if (ok)
            *ok = true;
        return static_cast<qint64>(std::llround(value.toDouble() * 100.0));
    }
    if (ok)
        *ok = false;
    return 0;
}

} // namespace

EnableBankingProvider::EnableBankingProvider(QObject *parent)
    : Provider(parent)
    , m_http(new HttpClient(this))
    , m_listener(new RedirectListener(this))
{
    m_http->setBaseUrl(QUrl(m_settings.apiBaseUrl()));

    connect(m_listener, &RedirectListener::codeReceived, this,
            [this](const QString &code, const QString &state) {
                m_listener->stop();
                if (!m_expectedState.isEmpty() && state != m_expectedState) {
                    emit failed(tr("The bank's reply did not match this request "
                                   "(state mismatch). Nothing was connected."));
                    return;
                }
                createSession(code);
            });

    connect(m_listener, &RedirectListener::authorisationFailed, this,
            [this](const QString &message) {
                m_listener->stop();
                emit failed(tr("The bank refused the connection: %1").arg(message));
            });
}

bool EnableBankingProvider::refreshToken()
{
    if (!m_settings.isConfigured()) {
        emit failed(tr("Add your Enable Banking application id and private key in Settings "
                       "before connecting a bank."));
        return false;
    }

    QString errorMessage;
    const QByteArray key = m_settings.readPrivateKey(&errorMessage);
    if (key.isEmpty()) {
        emit failed(errorMessage);
        return false;
    }

    const QString token = Jwt::signRs256(key,
                                         m_settings.applicationId(),
                                         QLatin1String(kIssuer),
                                         QLatin1String(kAudience),
                                         kTokenTtlSeconds,
                                         &errorMessage);
    if (token.isEmpty()) {
        emit failed(errorMessage);
        return false;
    }

    m_http->setBaseUrl(QUrl(m_settings.apiBaseUrl()));
    m_http->setBearerToken(token.toUtf8());
    return true;
}

void EnableBankingProvider::loadBanks(const QString &country)
{
    if (!refreshToken())
        return;

    emit progress(tr("Loading banks for %1…").arg(country));

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("country"), country);
    query.addQueryItem(QStringLiteral("psu_type"), QStringLiteral("personal"));

    m_http->get(QStringLiteral("/aspsps"), query, [this](const HttpClient::Response &response) {
        if (!response.ok) {
            emit failed(tr("Could not load the bank list: %1").arg(response.errorMessage));
            return;
        }

        const QJsonArray aspsps = response.body.object().value(QStringLiteral("aspsps")).toArray();
        QVariantList banks;
        banks.reserve(aspsps.size());
        for (const QJsonValue &value : aspsps) {
            const QJsonObject aspsp = value.toObject();
            banks.append(QVariantMap{
                {QStringLiteral("name"), jsonString(aspsp, "name")},
                {QStringLiteral("country"), jsonString(aspsp, "country")},
                {QStringLiteral("logo"), jsonString(aspsp, "logo")},
                {QStringLiteral("psuTypes"),
                 aspsp.value(QStringLiteral("psu_types")).toVariant()},
            });
        }

        emit banksLoaded(banks);
        emit progress(tr("%n bank(s) available", nullptr, static_cast<int>(banks.size())));
    });
}

void EnableBankingProvider::connectBank(const QString &aspspName, const QString &country)
{
    if (!refreshToken())
        return;

    const quint16 port = m_settings.redirectPort();
    QString listenError;
    if (!m_listener->start(port, &listenError)) {
        emit failed(listenError);
        return;
    }

    m_expectedState = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_pendingInstitution = aspspName;

    const QString validUntil =
        QDateTime::currentDateTimeUtc().addDays(kConsentDays).toString(Qt::ISODate);

    const QJsonObject body{
        {QStringLiteral("access"), QJsonObject{{QStringLiteral("valid_until"), validUntil}}},
        {QStringLiteral("aspsp"),
         QJsonObject{{QStringLiteral("name"), aspspName}, {QStringLiteral("country"), country}}},
        {QStringLiteral("state"), m_expectedState},
        {QStringLiteral("redirect_url"), RedirectListener::redirectUrlForPort(port)},
        {QStringLiteral("psu_type"), QStringLiteral("personal")},
    };

    emit progress(tr("Asking %1 for consent…").arg(aspspName));

    m_http->post(QStringLiteral("/auth"),
                 QJsonDocument(body),
                 [this, aspspName](const HttpClient::Response &response) {
                     if (!response.ok) {
                         m_listener->stop();
                         emit failed(tr("%1 could not start the consent flow: %2")
                                         .arg(aspspName, response.errorMessage));
                         return;
                     }

                     const QUrl authUrl(
                         response.body.object().value(QStringLiteral("url")).toString());
                     if (!authUrl.isValid()) {
                         m_listener->stop();
                         emit failed(tr("The consent response did not contain a usable URL."));
                         return;
                     }

                     emit progress(tr("Complete the login for %1 in your browser…").arg(aspspName));
                     QDesktopServices::openUrl(authUrl);
                 });
}

void EnableBankingProvider::createSession(const QString &code)
{
    if (!refreshToken())
        return;

    emit progress(tr("Finishing the connection…"));

    m_http->post(QStringLiteral("/sessions"),
                 QJsonDocument(QJsonObject{{QStringLiteral("code"), code}}),
                 [this](const HttpClient::Response &response) {
                     if (!response.ok) {
                         emit failed(tr("Could not complete the connection: %1")
                                         .arg(response.errorMessage));
                         return;
                     }

                     const QString sessionId =
                         response.body.object().value(QStringLiteral("session_id")).toString();
                     if (sessionId.isEmpty()) {
                         emit failed(tr("The bank did not return a session id."));
                         return;
                     }

                     m_settings.addSessionId(sessionId);
                     loadSession(sessionId, true);
                 });
}

void EnableBankingProvider::loadStoredSessions()
{
    const QStringList sessions = m_settings.sessionIds();
    if (sessions.isEmpty()) {
        m_accounts.clear();
        emit accountsChanged();
        return;
    }
    if (!refreshToken())
        return;

    // Start from an empty list so a revoked session disappears on reload.
    m_accounts.clear();
    m_pendingSessionLoads = static_cast<int>(sessions.size());
    for (const QString &sessionId : sessions)
        loadSession(sessionId, false);
}

void EnableBankingProvider::loadSession(const QString &sessionId, bool announceConnection)
{
    m_http->get(QStringLiteral("/sessions/%1").arg(sessionId),
                {},
                [this, sessionId, announceConnection](const HttpClient::Response &response) {
                    if (m_pendingSessionLoads > 0)
                        --m_pendingSessionLoads;

                    if (!response.ok) {
                        // A session expires when its consent lapses; drop it so
                        // the user is prompted to reconnect rather than seeing
                        // the same error on every sync.
                        m_settings.removeSessionId(sessionId);
                        emit failed(tr("A bank connection has expired and was removed: %1")
                                        .arg(response.errorMessage));
                        emit accountsChanged();
                        return;
                    }

                    const QJsonObject session = response.body.object();
                    const QString institution =
                        session.value(QStringLiteral("aspsp")).toObject()
                            .value(QStringLiteral("name")).toString();

                    QList<AccountModel::Account> accounts = m_accounts.accounts();
                    const QJsonArray sessionAccounts =
                        session.value(QStringLiteral("accounts")).toArray();

                    for (const QJsonValue &value : sessionAccounts) {
                        const AccountModel::Account account = parseAccount(
                            value,
                            sessionId,
                            institution.isEmpty() ? m_pendingInstitution : institution);
                        if (!account.uid.isEmpty())
                            accounts.append(account);
                    }

                    m_accounts.setAccounts(accounts);
                    emit accountsChanged();

                    // One request per account, after the accounts themselves
                    // are on screen, so the list appears immediately and the
                    // balances fill in as they arrive.
                    for (const AccountModel::Account &account : accounts)
                        fetchBalance(account);

                    if (announceConnection) {
                        emit connected(institution.isEmpty() ? m_pendingInstitution : institution);
                        emit progress(tr("Connected %1").arg(
                            institution.isEmpty() ? m_pendingInstitution : institution));
                    }
                });
}

void EnableBankingProvider::fetchBalance(const AccountModel::Account &account)
{
    const QString uid = account.uid;
    m_http->get(QStringLiteral("/accounts/%1/balances").arg(uid), {},
                [this, uid](const HttpClient::Response &response) {
                    // Silent on failure: a bank that does not serve balances is
                    // not a problem to report on every launch, and the account
                    // simply shows no figure.
                    if (!response.ok)
                        return;

                    QList<Balance::Reported> reported;
                    const QJsonArray balances =
                        response.body.object().value(QStringLiteral("balances")).toArray();
                    for (const QJsonValue &value : balances) {
                        const QJsonObject object = value.toObject();
                        const QJsonObject amount =
                            object.value(QStringLiteral("balance_amount")).toObject();

                        Balance::Reported entry;
                        entry.name = object.value(QStringLiteral("name")).toString();
                        entry.type = object.value(QStringLiteral("balance_type")).toString();
                        entry.currency = amount.value(QStringLiteral("currency")).toString();
                        // A string, not a number: the API sends decimal amounts
                        // as text precisely so they survive the trip intact.
                        entry.amountMinor = Money::fromDecimalString(
                            amount.value(QStringLiteral("amount")).toString());
                        entry.referenceDate = QDate::fromString(
                            object.value(QStringLiteral("reference_date")).toString(),
                            Qt::ISODate);
                        if (entry.isValid())
                            reported.append(entry);
                    }

                    const Balance::Reported chosen = Balance::preferred(reported);
                    if (!chosen.isValid())
                        return;

                    AccountModel::Account update;
                    update.hasBalance = true;
                    update.balanceMinor = chosen.amountMinor;
                    update.balanceCurrency = chosen.currency;
                    update.balanceName = chosen.name;
                    update.balanceType = chosen.type;
                    update.balanceDate = chosen.referenceDate;
                    m_accounts.setBalance(uid, update);
                    emit accountsChanged();
                });
}

void EnableBankingProvider::fetch(const QDate &from, const QDate &to)
{
    const QList<AccountModel::Account> accounts = m_accounts.accounts();
    if (accounts.isEmpty()) {
        emit failed(tr("No bank accounts are connected yet."));
        return;
    }
    if (!refreshToken())
        return;

    m_outstandingFetches = static_cast<int>(accounts.size());
    m_fetchedRows = 0;
    m_fetchErrors.clear();

    emit progress(tr("Fetching transactions from %n account(s)…", nullptr,
                     static_cast<int>(accounts.size())));

    for (const AccountModel::Account &account : accounts) {
        fetchAccountPage(account, from, to, QString());
        // Balances move whenever transactions do, so a sync refreshes both.
        fetchBalance(account);
    }
}

void EnableBankingProvider::fetchAccountPage(const AccountModel::Account &account,
                                             const QDate &from,
                                             const QDate &to,
                                             const QString &continuationKey,
                                             bool narrowed)
{
    QUrlQuery query;
    if (from.isValid())
        query.addQueryItem(QStringLiteral("date_from"), from.toString(Qt::ISODate));
    if (to.isValid())
        query.addQueryItem(QStringLiteral("date_to"), to.toString(Qt::ISODate));
    if (!continuationKey.isEmpty())
        query.addQueryItem(QStringLiteral("continuation_key"), continuationKey);

    m_http->get(QStringLiteral("/accounts/%1/transactions").arg(account.uid),
                query,
                [this, account, from, to, narrowed](const HttpClient::Response &response) {
                    if (!response.ok) {
                        constexpr int unprocessable = 422;
                        if (response.httpStatus == unprocessable && !narrowed) {
                            const SyncWindow retry = SyncWindow::narrowed(
                                SyncWindow{from, to}, QDate::currentDate());
                            if (retry.isValid()) {
                                fetchAccountPage(account, retry.from, retry.to, QString(), true);
                                return;
                            }
                            // Nothing narrower to ask for: the whole period
                            // predates what this bank will serve. Said plainly,
                            // because "wrong transactions period requested"
                            // gives the user nothing to act on.
                            m_fetchErrors.append(
                                tr("%1: this bank will not serve transactions from that far "
                                   "back. Reconnect it to refresh the consent.")
                                    .arg(accountLabel(account)));
                            accountFetchFinished();
                            return;
                        }
                        m_fetchErrors.append(
                            tr("%1: %2").arg(accountLabel(account), response.errorMessage));
                        accountFetchFinished();
                        return;
                    }

                    const QJsonObject payload = response.body.object();
                    const TransactionList transactions = parseTransactions(
                        payload.value(QStringLiteral("transactions")).toArray(), account);

                    if (!transactions.isEmpty()) {
                        m_fetchedRows += static_cast<int>(transactions.size());
                        emit transactionsReady(transactions);
                    }

                    const QString nextKey =
                        payload.value(QStringLiteral("continuation_key")).toString();
                    if (!nextKey.isEmpty()) {
                        fetchAccountPage(account, from, to, nextKey, narrowed);
                        return;
                    }

                    accountFetchFinished();
                });
}

void EnableBankingProvider::accountFetchFinished()
{
    if (m_outstandingFetches > 0)
        --m_outstandingFetches;
    if (m_outstandingFetches > 0)
        return;

    if (!m_fetchErrors.isEmpty() && m_fetchedRows == 0) {
        emit failed(m_fetchErrors.join(QLatin1String("; ")));
        return;
    }

    QString summary = tr("Synced %n transaction(s)", nullptr, m_fetchedRows);
    if (!m_fetchErrors.isEmpty()) {
        summary += tr(" — but %n account(s) failed: %1", nullptr,
                      static_cast<int>(m_fetchErrors.size()))
                       .arg(m_fetchErrors.join(QLatin1String("; ")));
    }
    emit finished(summary);
}

AccountModel::Account EnableBankingProvider::parseAccount(const QJsonValue &value,
                                                          const QString &sessionId,
                                                          const QString &institution)
{
    AccountModel::Account account;
    account.sessionId = sessionId;
    account.institution = institution;

    // Accounts arrive either as bare uid strings or as full objects depending
    // on the endpoint version.
    if (value.isString()) {
        account.uid = value.toString();
        return account;
    }

    const QJsonObject object = value.toObject();
    account.uid = jsonString(object, "uid");
    account.name = jsonString(object, "name");
    account.product = jsonString(object, "product");
    account.currency = jsonString(object, "currency");
    account.cashAccountType = jsonString(object, "cash_account_type");

    /*
     * A credit card has no IBAN. The bank identifies it under account_id.other
     * instead, typically with scheme_name CPAN or a masked PAN, so falling back
     * there is what stops a card account showing up blank and unidentifiable.
     */
    const QJsonObject accountId = object.value(QStringLiteral("account_id")).toObject();
    account.identifier = accountId.value(QStringLiteral("iban")).toString();
    if (account.identifier.isEmpty()) {
        const QJsonObject other = accountId.value(QStringLiteral("other")).toObject();
        account.identifier = other.value(QStringLiteral("identification")).toString();
    }
    if (account.identifier.isEmpty())
        account.identifier = jsonString(object, "masked_pan");

    if (account.name.isEmpty())
        account.name = account.product;
    if (account.name.isEmpty() && account.isCard())
        account.name = QObject::tr("Credit card");
    if (account.name.isEmpty())
        account.name = account.institution;

    return account;
}

TransactionList EnableBankingProvider::parseTransactions(
    const QJsonArray &array, const AccountModel::Account &account) const
{
    TransactionList transactions;
    transactions.reserve(array.size());
    int ordinal = 0;

    for (const QJsonValue &value : array) {
        const QJsonObject object = value.toObject();

        bool amountOk = false;
        const QJsonObject amountObject =
            object.value(QStringLiteral("transaction_amount")).toObject();
        const qint64 magnitude = readAmountMinor(amountObject, &amountOk);
        if (!amountOk)
            continue;

        Transaction transaction;
        transaction.source = sourceId();
        transaction.institution = account.institution;
        transaction.accountId = account.uid;
        transaction.accountName = account.name;

        /*
         * Direction. credit_debit_indicator is optional in the Berlin Group
         * schema: when the bank states it the reported amount is unsigned and
         * the indicator is authoritative, and when it is absent the amount
         * carries its own sign. Forcing everything without an indicator to be
         * a debit -- as this once did -- turns incoming money into spending.
         */
        const QString indicator = jsonString(object, "credit_debit_indicator");
        const bool isCredit = indicator.isEmpty()
                                  ? magnitude > 0
                                  : indicator.startsWith(QLatin1String("CRDT"),
                                                         Qt::CaseInsensitive);
        transaction.amountMinor = indicator.isEmpty()
                                      ? magnitude
                                      : (isCredit ? qAbs(magnitude) : -qAbs(magnitude));
        transaction.currency = jsonString(amountObject, "currency");
        if (transaction.currency.isEmpty())
            transaction.currency = account.currency;

        transaction.bookingDate = jsonDate(object, "booking_date");
        if (!transaction.bookingDate.isValid())
            transaction.bookingDate = jsonDate(object, "transaction_date");
        transaction.valueDate = jsonDate(object, "value_date");
        if (!transaction.bookingDate.isValid())
            transaction.bookingDate = transaction.valueDate;
        if (!transaction.bookingDate.isValid())
            continue;

        // For money out the merchant is the creditor; for money in it is the
        // debtor. Fall back to the other party if the expected one is absent.
        const QString creditorName =
            object.value(QStringLiteral("creditor")).toObject()
                .value(QStringLiteral("name")).toString();
        const QString debtorName =
            object.value(QStringLiteral("debtor")).toObject()
                .value(QStringLiteral("name")).toString();
        transaction.counterparty = isCredit ? debtorName : creditorName;
        if (transaction.counterparty.isEmpty())
            transaction.counterparty = isCredit ? creditorName : debtorName;

        QStringList remittance;
        for (const QJsonValue &line :
             object.value(QStringLiteral("remittance_information")).toArray()) {
            const QString text = line.toString().trimmed();
            if (!text.isEmpty())
                remittance.append(text);
        }
        transaction.description = remittance.join(QLatin1Char(' '));
        if (transaction.description.isEmpty())
            transaction.description = jsonString(object, "note");
        if (transaction.description.isEmpty())
            transaction.description = transaction.counterparty;

        transaction.reference = jsonString(object, "reference_number");
        transaction.status = jsonString(object, "status");

        const QString bankReference = !jsonString(object, "entry_reference").isEmpty()
                                          ? jsonString(object, "entry_reference")
                                          : jsonString(object, "transaction_id");

        transaction.id = makeTransactionId(transaction.source,
                                           transaction.accountId,
                                           bankReference,
                                           transaction.bookingDate,
                                           transaction.amountMinor,
                                           transaction.description,
                                           ordinal++);
        transactions.append(transaction);
    }

    return transactions;
}

void EnableBankingProvider::disconnectAll()
{
    m_settings.clearSessionIds();
    m_accounts.clear();
    emit accountsChanged();
    emit progress(tr("All bank connections were removed from this app."));
}
