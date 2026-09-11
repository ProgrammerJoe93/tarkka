#pragma once

#include "auth/Settings.h"
#include "core/Categoriser.h"
#include "data/AccountModel.h"
#include "data/SummaryModel.h"
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
    Q_PROPERTY(AccountModel *accounts READ accounts CONSTANT)

    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool statusIsError READ statusIsError NOTIFY statusChanged)

    Q_PROPERTY(QVariantList banks READ banks NOTIFY banksChanged)
    Q_PROPERTY(QStringList institutions READ institutions NOTIFY dataChanged)
    Q_PROPERTY(QStringList categories READ categories NOTIFY dataChanged)

    Q_PROPERTY(QDate dateFrom READ dateFrom WRITE setDateFrom NOTIFY filterChanged)
    Q_PROPERTY(QDate dateTo READ dateTo WRITE setDateTo NOTIFY filterChanged)
    Q_PROPERTY(QString institutionFilter READ institutionFilter WRITE setInstitutionFilter
                   NOTIFY filterChanged)
    Q_PROPERTY(QString categoryFilter READ categoryFilter WRITE setCategoryFilter
                   NOTIFY filterChanged)
    Q_PROPERTY(bool spendOnly READ spendOnly WRITE setSpendOnly NOTIFY filterChanged)
    Q_PROPERTY(QString rangeLabel READ rangeLabel NOTIFY filterChanged)

    Q_PROPERTY(bool configured READ isConfigured NOTIFY settingsChanged)
    Q_PROPERTY(QString applicationId READ applicationId NOTIFY settingsChanged)
    Q_PROPERTY(QString privateKeyPath READ privateKeyPath NOTIFY settingsChanged)
    Q_PROPERTY(int redirectPort READ redirectPort NOTIFY settingsChanged)
    Q_PROPERTY(QString country READ country NOTIFY settingsChanged)
    Q_PROPERTY(QString redirectUrl READ redirectUrl NOTIFY settingsChanged)
    Q_PROPERTY(bool trading212Configured READ isTrading212Configured NOTIFY settingsChanged)

public:
    explicit AppController(QObject *parent = nullptr);
    ~AppController() override;

    TransactionFilterModel *transactions() { return &m_filterModel; }
    SummaryModel *summary() { return &m_summaryModel; }
    AccountModel *accounts();

    bool isBusy() const { return m_busy; }
    QString status() const { return m_status; }
    bool statusIsError() const { return m_statusIsError; }

    QVariantList banks() const { return m_banks; }
    QStringList institutions() const;
    QStringList categories() const;

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
    QString rangeLabel() const;

    bool isConfigured() const { return m_settings.isConfigured(); }
    QString applicationId() const { return m_settings.applicationId(); }
    QString privateKeyPath() const { return m_settings.privateKeyPath(); }
    int redirectPort() const { return m_settings.redirectPort(); }
    QString country() const { return m_settings.country(); }
    QString redirectUrl() const;
    bool isTrading212Configured() const;

    //! Shifts the range to a whole month offset from the current one.
    Q_INVOKABLE void shiftMonth(int months);
    //! Sets the range to the last \a days days, ending today.
    Q_INVOKABLE void setLastDays(int days);

    Q_INVOKABLE void loadBanks();
    Q_INVOKABLE void connectBank(const QString &bankName, const QString &countryCode);
    Q_INVOKABLE void syncNow();
    Q_INVOKABLE void importStatement(const QUrl &fileUrl, const QString &institutionHint = {});
    Q_INVOKABLE void setTransactionCategory(const QString &transactionId, const QString &category);
    Q_INVOKABLE void disconnectAll();
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
    Q_INVOKABLE void saveTrading212Key(const QString &apiKey);
    Q_INVOKABLE void copyToClipboard(const QString &text);
    Q_INVOKABLE void dismissStatus();

signals:
    void busyChanged();
    void statusChanged();
    void banksChanged();
    void dataChanged();
    void filterChanged();
    void settingsChanged();

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

    Settings m_settings;
    Categoriser m_categoriser;
    TransactionStore m_store;
    TransactionStore::Filter m_filter;

    TransactionModel m_transactionModel;
    TransactionFilterModel m_filterModel;
    SummaryModel m_summaryModel;

    EnableBankingProvider *m_bank = nullptr;
    CsvProvider *m_csv = nullptr;
    Trading212Provider *m_trading212 = nullptr;

    QVariantList m_banks;
    QString m_status;
    bool m_statusIsError = false;
    bool m_busy = false;
    int m_activeTasks = 0;
};
