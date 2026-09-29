#include "app/AppController.h"

#include "core/Money.h"
#include "core/Recurring.h"

#include "data/AccountModel.h"
#include "providers/CsvProvider.h"
#include "providers/Provider.h"
#include "auth/RedirectListener.h"
#include "providers/EnableBankingProvider.h"
#include "providers/Trading212Provider.h"
#include "i18n/Translation.h"
#include "auth/ConnectionFile.h"

#include <QClipboard>
#include <QFile>
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

    QString budgetError;
    if (!m_budgets.loadFromFile(Settings::budgetsPath(), &budgetError))
        report(budgetError, true);

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
    connect(m_trading212, &Trading212Provider::cashReported, this, [this](qint64 freeMinor) {
        m_trading212Cash = freeMinor;
        emit trading212CashChanged();
    });
    connect(m_bank, &EnableBankingProvider::accountsChanged, this, [this] {
        emit dataChanged();
        // Balances arrive one account at a time after the list is already
        // shown, so the totals have to be re-read as each lands.
        emit accountsChanged();
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
    if (m_activeTasks > 0)
        return;

    if (m_busy) {
        m_busy = false;
        emit busyChanged();
    }

    /*
     * Rules are re-applied once every provider has finished rather than per
     * row as it arrives, so a rule added after a row was first stored still
     * claims it. Once at the end rather than once per provider: three banks
     * syncing together would otherwise walk the table three times for the same
     * answer.
     *
     * Only rows the rules themselves categorised are revised; see
     * TransactionStore::recategorise.
     */
    QString errorMessage;
    const int moved = m_store.recategorise(m_categoriser, &errorMessage);
    if (moved < 0) {
        report(errorMessage, true);
        return;
    }
    if (moved > 0)
        reload();
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
    /*
     * Providers mark categoryFromProvider themselves, because only they can
     * tell an answer they are sure of from a starting point they guessed.
     * Inferring it here from "a category is present" got that wrong: Trading
     * 212 reports a raw merchant category on most card rows, and treating that
     * as authoritative put a thousand rows beyond the reach of the user's own
     * rules.
     */
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
    m_summaryModel.setTargets(m_budgets.all());
    refreshTrends();
    emit dataChanged();
}

void AppController::refreshTrends()
{
    /*
     * The window ends at the month being shown rather than at today, so
     * stepping back through the months moves the comparison with them: looking
     * at March should compare March with the months before it, not with now.
     */
    const QDate anchor = m_filter.from.isValid() ? m_filter.from : QDate::currentDate();
    const QDate lastMonth(anchor.year(), anchor.month(), 1);

    QList<QDate> months;
    months.reserve(trendMonths);
    for (int offset = trendMonths - 1; offset >= 0; --offset)
        months.append(lastMonth.addMonths(-offset));

    QString errorMessage;
    const QList<TransactionStore::MonthlySpend> spending = m_store.monthlySpend(
        months.constFirst(), lastMonth.addMonths(1).addDays(-1), m_filter.currency, &errorMessage);
    if (!errorMessage.isEmpty())
        report(errorMessage, true);

    m_trendsModel.setSpending(spending, months, m_filter.currency);
    m_trendsModel.setTargets(m_budgets.all());

    /*
     * Standing payments need the individual rows rather than monthly totals,
     * and several months of them: three occurrences is the least that is a
     * pattern rather than a coincidence. The same window as the comparison, so
     * both answer for the same stretch of time.
     */
    TransactionStore::Filter window;
    window.from = months.constFirst();
    window.to = lastMonth.addMonths(1).addDays(-1);
    window.currency = m_filter.currency;
    const TransactionList windowRows = m_store.query(window, &errorMessage);
    if (!errorMessage.isEmpty())
        report(errorMessage, true);
    m_recurringModel.setPayments(Recurring::find(windowRows, m_filter.currency),
                                 m_filter.currency);
}

void AppController::setBudget(const QString &category, const QString &amountText)
{
    const QString trimmed = amountText.trimmed();
    if (trimmed.isEmpty()) {
        m_budgets.setTarget(category, -1);
    } else {
        bool parsed = false;
        const qint64 minor = Money::fromDecimalString(trimmed, &parsed);
        if (!parsed || minor < 0) {
            report(tr("%1 is not an amount this can budget.").arg(trimmed), true);
            return;
        }
        m_budgets.setTarget(category, minor);
    }

    QString errorMessage;
    if (!m_budgets.saveToFile(Settings::budgetsPath(), &errorMessage)) {
        report(errorMessage, true);
        return;
    }

    m_summaryModel.setTargets(m_budgets.all());
    m_trendsModel.setTargets(m_budgets.all());
}

QStringList AppController::institutions() const
{
    return m_store.isOpen() ? m_store.institutions() : QStringList();
}

QStringList AppController::categories() const
{
    return m_store.isOpen() ? m_store.categories() : QStringList();
}

QString AppController::totalBalanceText() const
{
    qint64 total = 0;
    for (const AccountModel::Account &account : m_bank->accountModel()->accounts()) {
        if (account.hasBalance && account.balanceCurrency == m_filter.currency)
            total += account.balanceMinor;
    }
    return Money::format(total, m_filter.currency);
}

bool AppController::hasBalances() const
{
    for (const AccountModel::Account &account : m_bank->accountModel()->accounts()) {
        if (account.hasBalance && account.balanceCurrency == m_filter.currency)
            return true;
    }
    return false;
}

int AppController::balancesInOtherCurrencies() const
{
    int others = 0;
    for (const AccountModel::Account &account : m_bank->accountModel()->accounts()) {
        if (account.hasBalance && account.balanceCurrency != m_filter.currency)
            ++others;
    }
    return others;
}

QString AppController::trading212CashText() const
{
    if (m_trading212Cash < 0)
        return {};
    // The cash endpoint reports no currency, so it is taken from the rows
    // Trading 212 has already supplied rather than assumed.
    const QString currency = m_store.isOpen()
                                 ? m_store.dominantCurrency(QStringLiteral("Trading 212"))
                                 : QString();
    return Money::format(m_trading212Cash, currency);
}

int AppController::uncategorisedCount() const
{
    return m_store.isOpen() ? m_store.countInCategory(Categoriser::uncategorised()) : 0;
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

    const SyncWindow window =
        SyncWindow::forRange(m_filter.from, m_filter.to, QDate::currentDate());

    if (haveBank)
        m_bank->fetch(window.from, window.to);
    if (haveTrading212)
        m_trading212->fetch(window.from, window.to);
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

void AppController::exportConnection(const QUrl &fileUrl)
{
    ConnectionFile::Contents contents;
    contents.applicationId = m_settings.applicationId();
    contents.country = m_settings.country();
    contents.sessionIds = m_settings.sessionIds();

    if (contents.sessionIds.isEmpty()) {
        report(tr("There are no bank connections on this device to export."), true);
        return;
    }

    const QString path = fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString();
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        report(tr("Cannot write %1: %2").arg(path, file.errorString()), true);
        return;
    }
    file.write(ConnectionFile::serialise(contents));
    file.close();

    report(tr("Exported %n bank connection(s). Move the private key across separately.", nullptr,
              static_cast<int>(contents.sessionIds.size())),
           false);
}

void AppController::importConnection(const QUrl &fileUrl)
{
    const QString path = fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        report(tr("Cannot read %1: %2").arg(path, file.errorString()), true);
        return;
    }

    QString errorMessage;
    const ConnectionFile::Contents contents = ConnectionFile::parse(file.readAll(), &errorMessage);
    if (!contents.isValid()) {
        report(errorMessage, true);
        return;
    }

    // Merged rather than replacing: a bank connected only on this device would
    // otherwise vanish. A session the bank has since revoked is dropped by the
    // reload below, so stale entries do not accumulate.
    m_settings.setApplicationId(contents.applicationId);
    if (!contents.country.isEmpty())
        m_settings.setCountry(contents.country);
    for (const QString &sessionId : contents.sessionIds)
        m_settings.addSessionId(sessionId);

    emit settingsChanged();
    report(tr("Imported %n bank connection(s). Point Settings at the matching private key, "
              "then sync.", nullptr, static_cast<int>(contents.sessionIds.size())),
           false);
    m_bank->loadStoredSessions();
}

void AppController::setTransactionExcluded(const QString &transactionId, bool excluded)
{
    QString errorMessage;
    if (!m_store.setExcluded(transactionId, excluded, &errorMessage)) {
        report(errorMessage, true);
        return;
    }
    reload();
}

void AppController::addCategoryRule(const QString &merchant, const QString &category)
{
    const QString text = merchant.trimmed();
    if (text.isEmpty() || category.trimmed().isEmpty()) {
        report(tr("A rule needs both a merchant and a category."), true);
        return;
    }

    // Escaped: the merchant came from a transaction, so characters that happen
    // to be regex operators are literal parts of a name rather than syntax.
    Categoriser::Rule rule;
    rule.category = category.trimmed();
    rule.pattern = QRegularExpression(QRegularExpression::escape(text),
                                      QRegularExpression::CaseInsensitiveOption);

    // Replaces any rule already matching this merchant rather than adding a
    // second that disagrees with it; see Categoriser::setRule.
    m_categoriser.setRule(rule);

    QString errorMessage;
    if (!m_categoriser.saveToFile(Settings::categoryRulesPath(), &errorMessage)) {
        report(errorMessage, true);
        return;
    }

    /*
     * Applied to what is already stored, or the rule would only affect rows
     * that have yet to arrive and the screen would not change. Rows the user
     * categorised by hand are left alone: an explicit decision on one row
     * outranks a rule, even a rule the same user just wrote.
     */
    const int changed = m_store.recategorise(m_categoriser, &errorMessage);
    if (changed < 0) {
        report(errorMessage, true);
        return;
    }

    reload();
    emit dataChanged();
    report(tr("%n transaction(s) moved to %1.", nullptr, changed).arg(rule.category), false);
}

void AppController::resetCategoryRules()
{
    m_categoriser.setRules(Categoriser::defaultRules());

    QString errorMessage;
    if (!m_categoriser.saveToFile(Settings::categoryRulesPath(), &errorMessage)) {
        report(errorMessage, true);
        return;
    }

    const int moved = m_store.recategorise(m_categoriser, &errorMessage);
    if (moved < 0) {
        report(errorMessage, true);
        return;
    }

    reload();
    emit dataChanged();
    report(tr("Category rules reset. %n transaction(s) moved.", nullptr, moved), false);
}

void AppController::deleteAllTransactions()
{
    QString errorMessage;
    if (!m_store.clear(&errorMessage)) {
        report(errorMessage, true);
        return;
    }

    reload();
    emit dataChanged();
    report(tr("All stored transactions were deleted. Bank connections were kept."), false);
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
    if (redirectPort > 0 && redirectPort <= 65535)
        m_settings.setRedirectPort(static_cast<quint16>(redirectPort));
    if (!countryCode.isEmpty())
        m_settings.setCountry(countryCode);

    // Done last, and allowed to fail on its own: everything above is already
    // stored, so a key that cannot be read costs the user only the key rather
    // than the whole form.
    const QString trimmedPath = privateKeyPath.trimmed();
    QString keyError;
    if (!trimmedPath.isEmpty()) {
        const QUrl asUrl(trimmedPath);
        if (!m_settings.setPrivateKeyPath(
                asUrl.isLocalFile() ? asUrl.toLocalFile() : trimmedPath, &keyError)) {
            emit settingsChanged();
            report(keyError, true);
            return;
        }
    }

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
