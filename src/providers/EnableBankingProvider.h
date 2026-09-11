#pragma once

#include "auth/Settings.h"
#include "data/AccountModel.h"
#include "providers/Provider.h"

#include <QDate>
#include <QVariantList>

class HttpClient;
class RedirectListener;

/*!
 * Live account access through the Enable Banking aggregation API.
 *
 * This is the route a private individual can actually take to their own real
 * data. Reading a Nordea or Revolut account directly under PSD2 requires being
 * a licensed AISP with an eIDAS certificate, and Revolut has no personal-account
 * API at all; Enable Banking is a licensed intermediary whose "restricted
 * production" mode lets an ordinary developer link their *own* accounts and
 * read them live. Both banks are reached through the same code path here.
 *
 * The application authenticates with a short-lived RS256 JWT, then each bank is
 * connected once through a browser consent flow that yields a session covering
 * one or more accounts.
 */
class EnableBankingProvider : public Provider
{
    Q_OBJECT

public:
    explicit EnableBankingProvider(QObject *parent = nullptr);

    QString sourceId() const override { return QStringLiteral("enablebanking"); }
    QString displayName() const override { return QStringLiteral("Enable Banking"); }

    AccountModel *accountModel() { return &m_accounts; }

    /*!
     * Rebuilds the bearer token from the configured application id and key.
     * Returns false and reports through failed() if the key cannot be used.
     */
    bool refreshToken();

    //! GET /aspsps -- the banks available for \a country.
    void loadBanks(const QString &country);

    //! Re-reads every stored session so the account list survives a restart.
    void loadStoredSessions();

    /*!
     * Begins the consent flow for one bank, opening the user's browser.
     *
     * The redirect target must be whitelisted in the Enable Banking control
     * panel; RedirectListener serves it on loopback.
     */
    void connectBank(const QString &aspspName, const QString &country);

    //! Fetches transactions for every linked account in the date range.
    void fetch(const QDate &from, const QDate &to);

    //! Forgets every stored session. Does not revoke consent at the bank.
    void disconnectAll();

signals:
    void banksLoaded(const QVariantList &banks);
    void accountsChanged();
    void connected(const QString &institution);

private:
    void createSession(const QString &code);
    void loadSession(const QString &sessionId, bool announceConnection);
    void fetchAccountPage(const AccountModel::Account &account,
                          const QDate &from,
                          const QDate &to,
                          const QString &continuationKey);
    void accountFetchFinished();
    TransactionList parseTransactions(const QJsonArray &array,
                                      const AccountModel::Account &account) const;

    Settings m_settings;
    HttpClient *m_http;
    RedirectListener *m_listener;
    AccountModel m_accounts;

    QString m_expectedState;
    QString m_pendingInstitution;

    // Fetches fan out across accounts; the run finishes when the last returns.
    int m_outstandingFetches = 0;
    int m_fetchedRows = 0;
    QStringList m_fetchErrors;
    int m_pendingSessionLoads = 0;
};
