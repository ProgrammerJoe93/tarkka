#include "app/AppController.h"

#include "data/AccountModel.h"
#include "providers/CsvProvider.h"
#include "auth/RedirectListener.h"
#include "providers/EnableBankingProvider.h"
#include "providers/Trading212Provider.h"
#include "i18n/Translation.h"

#include <QClipboard>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLocale>
#include <QUrl>

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_bank(new EnableBankingProvider(this))
    , m_csv(new CsvProvider(this))
    , m_trading212(new Trading212Provider(this))
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
            endTask();
            report(summary, false);
            reload();
        });
        connect(provider, &Provider::failed, this, [this](const QString &message) {
            endTask();
            report(message, true);
        });
    };
    connectProvider(m_bank);
    connectProvider(m_csv);
    connectProvider(m_trading212);

    connect(m_bank, &EnableBankingProvider::banksLoaded, this,
            [this](const QVariantList &banks) {
                endTask();
                m_banks = banks;
                emit banksChanged();
            });
    connect(m_bank, &EnableBankingProvider::accountsChanged, this, [this] {
        emit dataChanged();
    });
    connect(m_bank, &EnableBankingProvider::connected, this, [this] {
        // The consent flow ends here rather than with finished(), so close out
        // the task connectBank() opened before starting the follow-up sync.
        endTask();
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

void AppController::beginTask()
{
    ++m_activeTasks;
    if (!m_busy) {
        m_busy = true;
        emit busyChanged();
    }
}

void AppController::endTask()
{
    m_activeTasks = qMax(0, m_activeTasks - 1);
    if (m_activeTasks == 0 && m_busy) {
        m_busy = false;
        emit busyChanged();
    }
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
    // Minor units only add up within one currency, so the view is always
    // pinned to exactly one. Default to the most-used rather than showing a
    // meaningless mixed total.
    if (m_filter.currency.isEmpty()) {
        const QStringList available = m_store.currencies();
        if (!available.isEmpty()) {
            m_filter.currency = available.first();
            emit filterChanged();
        }
    }

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

QStringList AppController::currencies() const
{
    return m_store.isOpen() ? m_store.currencies() : QStringList();
}

void AppController::setCurrencyFilter(const QString &currency)
{
    if (m_filter.currency == currency)
        return;
    m_filter.currency = currency;
    emit filterChanged();
    reload();
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
    // The default locale, not the system one: it tracks the chosen language.
    const QLocale locale;
    const bool wholeMonth = m_filter.from.isValid() && m_filter.to.isValid()
                            && m_filter.from.day() == 1
                            && m_filter.to == m_filter.from.addMonths(1).addDays(-1);
    if (wholeMonth) {
        // The standalone month name rather than what MMMM yields: Finnish
        // inflects the month inside a date ("1. syyskuuta") but a heading
        // wants the plain nominative ("syyskuu"). English spells both alike.
        return QStringLiteral("%1 %2")
            .arg(locale.standaloneMonthName(m_filter.from.month(), QLocale::LongFormat),
                 QString::number(m_filter.from.year()));
    }

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

void AppController::loadBanks(const QString &countryCode)
{
    beginTask();
    const QString country = countryCode.trimmed().isEmpty() ? m_settings.country()
                                                            : countryCode.trimmed().toUpper();
    m_bank->loadBanks(country);
}

void AppController::connectBank(const QString &bankName, const QString &countryCode)
{
    beginTask();
    m_bank->connectBank(bankName, countryCode.isEmpty() ? m_settings.country() : countryCode);
}

void AppController::syncNow()
{
    const bool haveBank = m_settings.isConfigured();
    const bool haveTrading212 = m_trading212->isConfigured();

    if (!haveBank && !haveTrading212) {
        report(tr("Add your Enable Banking or Trading 212 credentials in Settings first, "
                  "or import a statement file instead."),
               true);
        return;
    }

    if (haveBank)
        beginTask();
    if (haveTrading212)
        beginTask();

    // Fetch a little wider than the visible range so that rows a provider books
    // late still land in the month they belong to.
    const QDate from = m_filter.from.addDays(-7);
    const QDate to = m_filter.to.addDays(1);

    if (haveBank)
        m_bank->fetch(from, to);
    if (haveTrading212)
        m_trading212->fetch(from, to);
}

void AppController::importStatement(const QUrl &fileUrl,
                                    const QString &institutionHint,
                                    const QString &defaultCurrency)
{
    const QString path = fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString();
    beginTask();
    m_csv->importFile(path, institutionHint, defaultCurrency);
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

void AppController::setAppearance(const QString &mode)
{
    // Compared after the write rather than before it: the setter normalises,
    // so an unrecognised mode can leave the stored value exactly as it was.
    const QString previous = m_settings.appearance();
    m_settings.setAppearance(mode);
    if (m_settings.appearance() != previous)
        emit appearanceChanged();
}

void AppController::setLanguage(const QString &language)
{
    const QString previous = m_settings.language();
    m_settings.setLanguage(language);
    if (m_settings.language() == previous)
        return;

    Translation::apply(m_settings.language());
    // qsTr() results are cached in the bindings that produced them, so without
    // this the window keeps every string it was built with.
    if (QQmlEngine *engine = qmlEngine(this))
        engine->retranslate();
    // Dates and amounts are formatted into the models rather than bound
    // through qsTr(), so retranslating leaves them in the old locale; this
    // re-reads them under the new one.
    reload();
    emit filterChanged();
    emit languageChanged();
}

QString AppController::redirectUrl() const
{
    return RedirectListener::redirectUrlForPort(m_settings.redirectPort());
}

bool AppController::isTrading212Configured() const
{
    return m_trading212->isConfigured();
}

void AppController::saveTrading212Credentials(const QString &apiKey, const QString &apiSecret)
{
    const bool clearing = apiKey.trimmed().isEmpty() && apiSecret.trimmed().isEmpty();
    if (!clearing && (apiKey.trimmed().isEmpty() || apiSecret.trimmed().isEmpty())) {
        report(tr("Trading 212 needs both the API key and the API secret."), true);
        return;
    }

    QString errorMessage;
    if (!m_settings.setTrading212Credentials(apiKey, apiSecret, &errorMessage)) {
        report(errorMessage, true);
        return;
    }
    emit settingsChanged();
    report(clearing ? tr("Trading 212 credentials removed.")
                    : tr("Trading 212 credentials saved."),
           false);
}

void AppController::copyToClipboard(const QString &text)
{
    if (QClipboard *clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(text);
        report(tr("Copied to clipboard."), false);
    }
}
