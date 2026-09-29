#pragma once

#include "auth/Settings.h"
#include "core/Budgets.h"
#include "core/Categoriser.h"
#include "data/AccountModel.h"
#include "data/SummaryModel.h"
#include "data/RecurringModel.h"
#include "data/TrendsModel.h"
#include "data/TransactionFilterModel.h"
#include "data/TransactionModel.h"
#include "data/TransactionStore.h"

#include <QDate>
#include <QObject>
#include <QQmlEngine>
#include <QStringList>
#include <QVariantList>

class CsvProvider;
class Trading212Provider;
class EnableBankingProvider;

/*!
 * The single object QML talks to.
 *
 * It owns the store, the models and the providers, and turns the asynchronous
 * provider signals into the small set of properties the UI binds against.
 */
class AppController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(TransactionFilterModel *transactions READ transactions CONSTANT)
    Q_PROPERTY(SummaryModel *summary READ summary CONSTANT)
    Q_PROPERTY(TrendsModel *trends READ trends CONSTANT)
    Q_PROPERTY(RecurringModel *recurring READ recurring CONSTANT)
    Q_PROPERTY(AccountModel *accounts READ accounts CONSTANT)

    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool statusIsError READ statusIsError NOTIFY statusChanged)

    Q_PROPERTY(QVariantList banks READ banks NOTIFY banksChanged)
    Q_PROPERTY(QStringList institutions READ institutions NOTIFY dataChanged)
    Q_PROPERTY(QStringList categories READ categories NOTIFY dataChanged)
    /*!
     * Rows no rule claimed, across all time.
     *
     * Surfaced as a count so the rule set can be kept healthy: uncategorised
     * rows are invisible in a total and quietly make every category wrong.
     */
    Q_PROPERTY(int uncategorisedCount READ uncategorisedCount NOTIFY dataChanged)
    Q_PROPERTY(QStringList currencies READ currencies NOTIFY dataChanged)

    Q_PROPERTY(QDate dateFrom READ dateFrom WRITE setDateFrom NOTIFY filterChanged)
    Q_PROPERTY(QDate dateTo READ dateTo WRITE setDateTo NOTIFY filterChanged)
    Q_PROPERTY(QString institutionFilter READ institutionFilter WRITE setInstitutionFilter
                   NOTIFY filterChanged)
    Q_PROPERTY(QString categoryFilter READ categoryFilter WRITE setCategoryFilter
                   NOTIFY filterChanged)
    Q_PROPERTY(bool spendOnly READ spendOnly WRITE setSpendOnly NOTIFY filterChanged)
    Q_PROPERTY(QString currencyFilter READ currencyFilter WRITE setCurrencyFilter
                   NOTIFY filterChanged)
    Q_PROPERTY(QString rangeLabel READ rangeLabel NOTIFY filterChanged)

    /*!
     * What the linked accounts currently hold, in the displayed currency.
     *
     * Only accounts reporting that currency are added up, and
     * \c balancesInOtherCurrencies counts the rest, for the same reason the
     * spending totals refuse to mix: a number made of euros and pounds is not
     * money. An account whose bank serves no balance is counted in neither.
     */
    Q_PROPERTY(QString totalBalanceText READ totalBalanceText NOTIFY accountsChanged)
    Q_PROPERTY(bool hasBalances READ hasBalances NOTIFY accountsChanged)
    Q_PROPERTY(int balancesInOtherCurrencies READ balancesInOtherCurrencies
                   NOTIFY accountsChanged)

    Q_PROPERTY(bool configured READ isConfigured NOTIFY settingsChanged)
    Q_PROPERTY(QString applicationId READ applicationId NOTIFY settingsChanged)
    Q_PROPERTY(QString privateKeyPath READ privateKeyPath NOTIFY settingsChanged)
    Q_PROPERTY(int redirectPort READ redirectPort NOTIFY settingsChanged)
    Q_PROPERTY(QString country READ country NOTIFY settingsChanged)
    Q_PROPERTY(QString redirectUrl READ redirectUrl NOTIFY settingsChanged)
    Q_PROPERTY(bool trading212Configured READ isTrading212Configured NOTIFY settingsChanged)
    /*!
     * Trading 212's uninvested cash, as last reported.
     *
     * The main pot only. Trading 212's public API covers the investing account
     * alone, so the card's spending pot is not available to read; it is
     * deliberately absent rather than approximated from something else.
     */
    Q_PROPERTY(QString trading212CashText READ trading212CashText NOTIFY trading212CashChanged)
    Q_PROPERTY(bool hasTrading212Cash READ hasTrading212Cash NOTIFY trading212CashChanged)
    Q_PROPERTY(QString appearance READ appearance WRITE setAppearance NOTIFY appearanceChanged)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)

public:
    explicit AppController(QObject *parent = nullptr);
    ~AppController() override;

    TransactionFilterModel *transactions() { return &m_filterModel; }
    SummaryModel *summary() { return &m_summaryModel; }
    TrendsModel *trends() { return &m_trendsModel; }
    RecurringModel *recurring() { return &m_recurringModel; }
    //! How many months the comparison spans, the shown month included.
    static constexpr int trendMonths = 6;
    AccountModel *accounts();

    bool isBusy() const { return m_busy; }
    QString status() const { return m_status; }
    bool statusIsError() const { return m_statusIsError; }

    QVariantList banks() const { return m_banks; }
    QStringList institutions() const;
    QStringList categories() const;
    int uncategorisedCount() const;
    //! The category name rows fall into when no rule matches.
    Q_INVOKABLE QString uncategorisedName() const { return Categoriser::uncategorised(); }
    QStringList currencies() const;

    QDate dateFrom() const { return m_filter.from; }
    void setDateFrom(const QDate &date);
    QDate dateTo() const { return m_filter.to; }
    void setDateTo(const QDate &date);
    QString institutionFilter() const { return m_filter.institution; }
    void setInstitutionFilter(const QString &institution);
    QString categoryFilter() const { return m_filter.category; }
    void setCategoryFilter(const QString &category);
    bool spendOnly() const { return m_filter.spendOnly; }
    void setSpendOnly(bool spendOnly);
    QString currencyFilter() const { return m_filter.currency; }
    void setCurrencyFilter(const QString &currency);
    QString rangeLabel() const;
    QString totalBalanceText() const;
    bool hasBalances() const;
    int balancesInOtherCurrencies() const;

    bool isConfigured() const { return m_settings.isConfigured(); }
    QString applicationId() const { return m_settings.applicationId(); }
    QString privateKeyPath() const { return m_settings.privateKeyPath(); }
    int redirectPort() const { return m_settings.redirectPort(); }
    QString country() const { return m_settings.country(); }
    QString redirectUrl() const;
    bool isTrading212Configured() const;
    QString trading212CashText() const;
    bool hasTrading212Cash() const { return m_trading212Cash >= 0; }
    QString appearance() const { return m_settings.appearance(); }
    /*!
     * Chooses the palette: "system", "light" or "dark".
     *
     * Kept apart from the settingsChanged() family because every colour in the
     * UI binds to it; folding it in would repaint the whole window each time a
     * bank credential is saved.
     */
    void setAppearance(const QString &mode);
    QString language() const { return m_settings.language(); }
    /*!
     * Chooses the interface language: "system", or a supported code.
     *
     * Applies immediately -- the window is retranslated in place rather than
     * asking for a restart, which for a two-language picker is a wait nobody
     * should have to sit through.
     */
    void setLanguage(const QString &language);

    //! Shifts the range to a whole month offset from the current one.
    Q_INVOKABLE void shiftMonth(int months);
    //! Sets the range to the last \a days days, ending today.
    Q_INVOKABLE void setLastDays(int days);

    /*!
     * Lists the banks available in \a countryCode.
     *
     * Accounts in several countries mean several ASPSP lists, so the country is
     * a parameter of the browse rather than a fixed setting; passing an empty
     * string falls back to the configured home country.
     */
    Q_INVOKABLE void loadBanks(const QString &countryCode = {});
    Q_INVOKABLE void connectBank(const QString &bankName, const QString &countryCode);
    Q_INVOKABLE void syncNow();
    /*!
     * Imports a statement file.
     *
     * \a defaultCurrency is used only when the file names no currency of its
     * own -- several UK exports do not -- so that sterling rows are never
     * quietly counted as euros.
     */
    Q_INVOKABLE void importStatement(const QUrl &fileUrl,
                                     const QString &institutionHint = {},
                                     const QString &defaultCurrency = {});
    Q_INVOKABLE void setTransactionCategory(const QString &transactionId, const QString &category);
    /*!
     * Leaves one row out of the totals, or puts it back.
     *
     * The row stays in the list either way -- marked, not hidden -- because a
     * row you cannot see is a row you cannot put back.
     */
    Q_INVOKABLE void setTransactionExcluded(const QString &transactionId, bool excluded);
    /*!
     * Sets the monthly target for \a category from what the user typed.
     *
     * An empty \a amountText clears the target, which is not the same as
     * budgeting zero: the first leaves the category unbudgeted, the second is a
     * decision to spend nothing on it.
     */
    Q_INVOKABLE void setBudget(const QString &category, const QString &amountText);
    /*!
     * Adds a rule putting everything matching \a merchant into \a category,
     * and applies it to the rows already stored.
     *
     * The rule file was previously editable only by hand, with a restart to
     * take effect -- which on a phone is no route at all, and the phone is
     * where a miscategorised row gets noticed. \a merchant is matched as plain
     * text rather than a pattern, because it comes from a row the user tapped
     * rather than from someone writing a regular expression.
     */
    Q_INVOKABLE void addCategoryRule(const QString &merchant, const QString &category);
    /*!
     * Throws away the rules and restores the built-in ones.
     *
     * Then re-applies them, so the effect is visible rather than waiting for
     * the next sync. Categories set by hand and by providers survive, as they
     * do any other re-application.
     */
    Q_INVOKABLE void resetCategoryRules();
    /*!
     * Deletes every stored transaction, leaving bank connections in place.
     *
     * Irreversible and not undone by syncing: a bank serves only the period its
     * consent covers, so anything older than that is gone for good.
     */
    Q_INVOKABLE void deleteAllTransactions();
    Q_INVOKABLE void disconnectAll();
    /*!
     * Writes this device's bank connections to \a fileUrl.
     *
     * Sessions belong to the Enable Banking application rather than to a
     * device, so importing the file elsewhere lets that device sync banks it
     * could not have connected itself. The private key is not included; see
     * ConnectionFile.
     */
    Q_INVOKABLE void exportConnection(const QUrl &fileUrl);
    //! Merges the connections in \a fileUrl into this device's.
    Q_INVOKABLE void importConnection(const QUrl &fileUrl);
    /*!
     * Persists the credentials.
     *
     * \a privateKeyPath accepts either a plain filesystem path typed by the
     * user or a file:// URL from the file dialog.
     */
    Q_INVOKABLE void saveCredentials(const QString &applicationId,
                                     const QString &privateKeyPath,
                                     int redirectPort,
                                     const QString &countryCode);
    Q_INVOKABLE void saveTrading212Credentials(const QString &apiKey,
                                               const QString &apiSecret);
    Q_INVOKABLE void copyToClipboard(const QString &text);
    Q_INVOKABLE void dismissStatus();

signals:
    void busyChanged();
    void statusChanged();
    void banksChanged();
    void dataChanged();
    void filterChanged();
    void settingsChanged();
    void accountsChanged();
    void trading212CashChanged();
    void appearanceChanged();
    void languageChanged();

private:
    /*!
     * Tracks how many provider runs are outstanding.
     *
     * Syncing hits every configured provider at once, so the busy flag has to
     * count rather than latch -- otherwise the first one to finish clears it
     * while the others are still working.
     */
    void beginTask();
    void endTask();
    void report(const QString &message, bool isError = false);
    void ingest(const TransactionList &transactions);
    void reload();
    //! Rebuilds the month-on-month comparison for the window ending at the shown month.
    void refreshTrends();

    Settings m_settings;
    Categoriser m_categoriser;
    Budgets m_budgets;
    TransactionStore m_store;
    TransactionStore::Filter m_filter;

    TransactionModel m_transactionModel;
    TransactionFilterModel m_filterModel;
    SummaryModel m_summaryModel;
    TrendsModel m_trendsModel;
    RecurringModel m_recurringModel;

    EnableBankingProvider *m_bank = nullptr;
    CsvProvider *m_csv = nullptr;
    Trading212Provider *m_trading212 = nullptr;

    QVariantList m_banks;
    QString m_status;
    bool m_statusIsError = false;
    bool m_busy = false;
    int m_activeTasks = 0;
    //! Uninvested cash in minor units; negative until a figure arrives.
    qint64 m_trading212Cash = -1;
};
