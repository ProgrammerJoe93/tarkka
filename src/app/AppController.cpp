#include "app/AppController.h"

#include "data/AccountModel.h"
#include "providers/CsvProvider.h"
#include "auth/RedirectListener.h"
#include "providers/EnableBankingProvider.h"

#include <QClipboard>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLocale>
#include <QUrl>

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_bank(new EnableBankingProvider(this))
    , m_csv(new CsvProvider(this))
{
    m_filterModel.setSourceModel(&m_transactionModel);

    QString errorMessage;
    if (!m_store.open(Settings::databasePath(), &errorMessage))
        report(errorMessage, true);

    // A rule file is written on first run so the categories are editable
    // without having to discover the format first.
    const QString rulesPath = Settings::categoryRulesPath();
    if (QFileInfo::exists(rulesPath)) {
        QString ruleError;
        if (!m_categoriser.loadFromFile(rulesPath, &ruleError))
            report(ruleError, true);
    } else {
        m_categoriser.saveToFile(rulesPath);
    }

    // Default to the current calendar month.
    const QDate today = QDate::currentDate();
    m_filter.from = QDate(today.year(), today.month(), 1);
    m_filter.to = m_filter.from.addMonths(1).addDays(-1);

    const auto connectProvider = [this](Provider *provider) {
        connect(provider, &Provider::transactionsReady, this, &AppController::ingest);
        connect(provider, &Provider::progress, this,
                [this](const QString &message) { report(message, false); });
        connect(provider, &Provider::finished, this, [this](const QString &summary) {
            setBusy(false);
            report(summary, false);
            reload();
        });
        connect(provider, &Provider::failed, this, [this](const QString &message) {
            setBusy(false);
            report(message, true);
        });
    };
    connectProvider(m_bank);
    connectProvider(m_csv);

    connect(m_bank, &EnableBankingProvider::banksLoaded, this,
            [this](const QVariantList &banks) {
                setBusy(false);
                m_banks = banks;
                emit banksChanged();
            });
    connect(m_bank, &EnableBankingProvider::accountsChanged, this, [this] {
        setBusy(false);
        emit dataChanged();
    });
    connect(m_bank, &EnableBankingProvider::connected, this, [this] {
        // A freshly linked bank has no rows here yet, so pull them immediately.
        syncNow();
    });

    reload();

    if (m_settings.isConfigured() && !m_settings.sessionIds().isEmpty())
        m_bank->loadStoredSessions();
}

AppController::~AppController() = default;

AccountModel *AppController::accounts()
{
    return m_bank->accountModel();
}

void AppController::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

void AppController::report(const QString &message, bool isError)
{
    if (message.isEmpty())
        return;
    m_status = message;
    m_statusIsError = isError;
    emit statusChanged();
}

void AppController::dismissStatus()
{
    m_status.clear();
    m_statusIsError = false;
    emit statusChanged();
}

void AppController::ingest(const TransactionList &transactions)
{
    TransactionList categorised = transactions;
    for (Transaction &transaction : categorised) {
        if (transaction.category.isEmpty())
            transaction.category = m_categoriser.categorise(transaction);
    }

    QString errorMessage;
    if (m_store.upsert(categorised, &errorMessage) < 0)
        report(errorMessage, true);
}

void AppController::reload()
{
    if (!m_store.isOpen())
        return;

    QString errorMessage;
    const TransactionList rows = m_store.query(m_filter, &errorMessage);
    if (!errorMessage.isEmpty())
        report(errorMessage, true);

    m_transactionModel.setTransactions(rows);
    m_summaryModel.setTransactions(rows, m_filter.from, m_filter.to);
    emit dataChanged();
}

QStringList AppController::institutions() const
{
    return m_store.isOpen() ? m_store.institutions() : QStringList();
}

QStringList AppController::categories() const
{
    return m_store.isOpen() ? m_store.categories() : QStringList();
}

void AppController::setDateFrom(const QDate &date)
{
    if (m_filter.from == date)
        return;
    m_filter.from = date;
    emit filterChanged();
    reload();
}

void AppController::setDateTo(const QDate &date)
{
    if (m_filter.to == date)
        return;
    m_filter.to = date;
    emit filterChanged();
    reload();
}

void AppController::setInstitutionFilter(const QString &institution)
{
    if (m_filter.institution == institution)
        return;
    m_filter.institution = institution;
    emit filterChanged();
    reload();
}

void AppController::setCategoryFilter(const QString &category)
{
    if (m_filter.category == category)
        return;
    m_filter.category = category;
    emit filterChanged();
    reload();
}

void AppController::setSpendOnly(bool spendOnly)
{
    if (m_filter.spendOnly == spendOnly)
        return;
    m_filter.spendOnly = spendOnly;
    emit filterChanged();
    reload();
}

QString AppController::rangeLabel() const
{
    const QLocale locale = QLocale::system();
    const bool wholeMonth = m_filter.from.isValid() && m_filter.to.isValid()
                            && m_filter.from.day() == 1
                            && m_filter.to == m_filter.from.addMonths(1).addDays(-1);
    if (wholeMonth)
        return locale.toString(m_filter.from, QStringLiteral("MMMM yyyy"));

    return QStringLiteral("%1 – %2")
        .arg(locale.toString(m_filter.from, QLocale::ShortFormat),
             locale.toString(m_filter.to, QLocale::ShortFormat));
}

void AppController::shiftMonth(int months)
{
    const QDate anchor = m_filter.from.isValid() ? m_filter.from : QDate::currentDate();
    const QDate first = QDate(anchor.year(), anchor.month(), 1).addMonths(months);
    m_filter.from = first;
    m_filter.to = first.addMonths(1).addDays(-1);
    emit filterChanged();
    reload();
}

void AppController::setLastDays(int days)
{
    m_filter.to = QDate::currentDate();
    m_filter.from = m_filter.to.addDays(-qMax(0, days - 1));
    emit filterChanged();
    reload();
}

void AppController::loadBanks()
{
    setBusy(true);
    m_bank->loadBanks(m_settings.country());
}

void AppController::connectBank(const QString &bankName, const QString &countryCode)
{
    setBusy(true);
    m_bank->connectBank(bankName, countryCode.isEmpty() ? m_settings.country() : countryCode);
}

void AppController::syncNow()
{
    if (!m_settings.isConfigured()) {
        report(tr("Add your Enable Banking application id and private key in Settings first, "
                  "or import a statement file instead."),
               true);
        return;
    }
    setBusy(true);
    // Fetch a little wider than the visible range so that rows the bank books
    // late still land in the month they belong to.
    m_bank->fetch(m_filter.from.addDays(-7), m_filter.to.addDays(1));
}

void AppController::importStatement(const QUrl &fileUrl, const QString &institutionHint)
{
    const QString path = fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString();
    setBusy(true);
    m_csv->importFile(path, institutionHint);
}

void AppController::setTransactionCategory(const QString &transactionId, const QString &category)
{
    QString errorMessage;
    if (!m_store.setCategory(transactionId, category, &errorMessage)) {
        report(errorMessage, true);
        return;
    }
    reload();
}

void AppController::disconnectAll()
{
    m_bank->disconnectAll();
    emit dataChanged();
}

void AppController::saveCredentials(const QString &applicationId,
                                    const QString &privateKeyPath,
                                    int redirectPort,
                                    const QString &countryCode)
{
    m_settings.setApplicationId(applicationId);

    const QString trimmedPath = privateKeyPath.trimmed();
    if (!trimmedPath.isEmpty()) {
        const QUrl asUrl(trimmedPath);
        m_settings.setPrivateKeyPath(asUrl.isLocalFile() ? asUrl.toLocalFile() : trimmedPath);
    }
    if (redirectPort > 0 && redirectPort <= 65535)
        m_settings.setRedirectPort(static_cast<quint16>(redirectPort));
    if (!countryCode.isEmpty())
        m_settings.setCountry(countryCode);

    emit settingsChanged();
    report(tr("Settings saved."), false);
}

QString AppController::redirectUrl() const
{
    return RedirectListener::redirectUrlForPort(m_settings.redirectPort());
}

void AppController::copyToClipboard(const QString &text)
{
    if (QClipboard *clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(text);
        report(tr("Copied to clipboard."), false);
    }
}
