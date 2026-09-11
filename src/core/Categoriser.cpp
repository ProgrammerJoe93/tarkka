#include "core/Categoriser.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

Categoriser::Rule makeRule(const QString &category, const QString &pattern)
{
    return Categoriser::Rule{
        category,
        QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption)};
}

} // namespace

Categoriser::Categoriser()
    : m_rules(defaultRules())
{
}

QList<Categoriser::Rule> Categoriser::defaultRules()
{
    return {
        makeRule(QStringLiteral("Groceries"),
                 QStringLiteral("k-market|k-supermarket|k-citymarket|prisma|s-market|alepa|sale |lidl|"
                                "food *market|ruokakauppa|sainsbury|tesco|coop|rimi|ica|willys|hemk")),
        makeRule(QStringLiteral("Restaurants & Cafes"),
                 QStringLiteral("ravintola|restaurant|cafe|kahvila|espresso|bar |pub |mcdonald|burger|"
                                "subway|pizza|kotipizza|hesburger|starbucks|wolt|foodora|deliveroo|"
                                "uber *eats|fazer")),
        makeRule(QStringLiteral("Transport"),
                 QStringLiteral("hsl|vr |vr-|matkahuolto|taksi|taxi|uber|bolt|onnibus|finnair|"
                                "norwegian|ryanair|sj |flixbus|parking|pysakointi|neste|abc |teboil|"
                                "st1|shell|circle *k")),
        makeRule(QStringLiteral("Housing & Utilities"),
                 QStringLiteral("vuokra|rent|asunto|taloyhtio|sahko|electricity|fortum|helen|caruna|"
                                "vesi|water|lampo|isannointi|mortgage|asuntolaina")),
        makeRule(QStringLiteral("Subscriptions"),
                 QStringLiteral("netflix|spotify|apple\\.com|apple *music|icloud|google *(one|storage)|"
                                "youtube|hbo|max\\b|disney|amazon *prime|patreon|substack|github|"
                                "openai|anthropic|claude|dropbox|adobe|steam")),
        makeRule(QStringLiteral("Telecom & Internet"),
                 QStringLiteral("elisa|dna |telia|saunalahti|moi |vodafone|three|o2 |broadband|laajakaista")),
        makeRule(QStringLiteral("Health & Pharmacy"),
                 QStringLiteral("apteekki|pharmacy|terveystalo|mehilainen|pihlajalinna|hammas|dental|"
                                "laakari|doctor|hospital|sairaala|optikko")),
        makeRule(QStringLiteral("Shopping"),
                 QStringLiteral("verkkokauppa|gigantti|power|clas *ohlson|ikea|jysk|zalando|hm\\b|h&m|"
                                "zara|uniqlo|amazon|ebay|aliexpress|temu|stockmann|sokos|prisma *verkko|"
                                "xxl|intersport|biltema|motonet|puuilo|tokmanni")),
        makeRule(QStringLiteral("Entertainment"),
                 QStringLiteral("finnkino|cinema|elokuva|teatteri|theatre|museo|museum|konsertti|"
                                "concert|ticketmaster|lippu\\.fi|tiketti|kuntosali|gym|elixia|fitness")),
        makeRule(QStringLiteral("Cash & ATM"),
                 QStringLiteral("otto\\.|atm|nosto|cash *withdrawal|kateisnosto")),
        makeRule(QStringLiteral("Fees & Interest"),
                 QStringLiteral("palvelumaksu|service *fee|korko|interest|fee\\b|provisio|maksu *kortista")),
        makeRule(QStringLiteral("Transfers"),
                 QStringLiteral("tilisiirto|own *transfer|omalle *tilille|to *revolut|from *revolut|"
                                "top-?up|siirto|transfer|"
                                // Paying off a credit card: money leaves the bank
                                // account and lands on the card as a credit.
                                "maksu *kortille|kortin *maksu|korttilasku|luottokorttilasku|"
                                // Trading 212 funds a spending card, so moving
                                // money there is a transfer, not an investment.
                                "trading *212|trading212|t212|"
                                "credit *card *(payment|repayment)|card *repayment|"
                                "payment *received *- *thank *you")),
        makeRule(QStringLiteral("Savings & Investing"),
                 QStringLiteral("nordnet|osuuspankki *saast|rahasto|fund|osake|share|etf|degiro|"
                                "coinbase|kraken|vault|saastotili")),
    };
}

void Categoriser::setRules(const QList<Rule> &rules)
{
    m_rules.clear();
    m_rules.reserve(rules.size());
    for (const Rule &rule : rules) {
        if (rule.pattern.isValid() && !rule.category.isEmpty())
            m_rules.append(rule);
    }
}

bool Categoriser::loadFromFile(const QString &path, QString *errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Cannot open %1: %2").arg(path, file.errorString());
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("%1 is not a JSON object: %2")
                                .arg(path, parseError.errorString());
        return false;
    }

    QList<Rule> loaded;
    const QJsonObject root = document.object();
    for (auto it = root.constBegin(); it != root.constEnd(); ++it) {
        const QJsonArray patterns = it.value().toArray();
        for (const QJsonValue &pattern : patterns) {
            const QString expression = pattern.toString();
            if (expression.isEmpty())
                continue;
            const Rule rule = makeRule(it.key(), expression);
            if (!rule.pattern.isValid()) {
                if (errorMessage)
                    *errorMessage = QStringLiteral("Invalid pattern for %1: %2")
                                        .arg(it.key(), rule.pattern.errorString());
                return false;
            }
            loaded.append(rule);
        }
    }

    setRules(loaded);
    return true;
}

bool Categoriser::saveToFile(const QString &path, QString *errorMessage) const
{
    QDir().mkpath(QFileInfo(path).absolutePath());

    QJsonObject root;
    for (const Rule &rule : m_rules) {
        QJsonArray patterns = root.value(rule.category).toArray();
        patterns.append(rule.pattern.pattern());
        root.insert(rule.category, patterns);
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Cannot write %1: %2").arg(path, file.errorString());
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

QString Categoriser::categorise(const Transaction &transaction) const
{
    const QString haystack = transaction.counterparty + QLatin1Char(' ')
                             + transaction.description + QLatin1Char(' ')
                             + transaction.reference;

    for (const Rule &rule : m_rules) {
        if (rule.pattern.match(haystack).hasMatch())
            return rule.category;
    }

    return transaction.amountMinor > 0 ? income() : uncategorised();
}
