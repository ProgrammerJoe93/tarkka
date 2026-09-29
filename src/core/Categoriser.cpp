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
                                "food *market|ruokakauppa|sainsbury|tesco|coop|rimi|ica|willys|hemk|"
                                // UK
                                "waitrose|asda|morrisons|aldi|iceland *food|ocado|m&s *food|marks *&? *spencer|budgens|londis|nisa|spar")),
        makeRule(QStringLiteral("Restaurants & Cafes"),
                 QStringLiteral("ravintola|restaurant|cafe|kahvila|espresso|bar |pub |mcdonald|burger|"
                                "subway|pizza|kotipizza|hesburger|starbucks|wolt|foodora|deliveroo|"
                                "uber *eats|fazer|"
                                // UK
                                "pret *a *manger|greggs|costa|caffe *nero|nando|wagamama|itsu|leon|five *guys|honest *burger|just *eat|gail")),
        makeRule(QStringLiteral("Transport"),
                 QStringLiteral("hsl|vr |vr-|matkahuolto|taksi|taxi|uber|bolt|onnibus|finnair|"
                                "norwegian|ryanair|sj |flixbus|parking|pysakointi|neste|abc |teboil|"
                                "st1|shell|circle *k|"
                                // UK
                                "tfl|transport *for *london|oyster|trainline|lner|gwr|northern *rail|national *rail|stagecoach|arriva|addison *lee|bp\\b|esso|texaco|sainsburys *petrol")),
        makeRule(QStringLiteral("Housing & Utilities"),
                 QStringLiteral("vuokra|rent|asunto|taloyhtio|sahko|electricity|fortum|helen|caruna|"
                                "vesi|water|lampo|isannointi|mortgage|asuntolaina|"
                                // UK
                                "council *tax|british *gas|octopus *energy|ovo *energy|edf *energy|e\\.on|scottish *power|thames *water|severn *trent|tv *licen[cs]e")),
        makeRule(QStringLiteral("Subscriptions"),
                 QStringLiteral("netflix|spotify|apple\\.com|apple *music|icloud|google *(one|storage)|"
                                "youtube|hbo|max\\b|disney|amazon *prime|patreon|substack|github|"
                                "openai|anthropic|claude|dropbox|adobe|steam")),
        makeRule(QStringLiteral("Telecom & Internet"),
                 QStringLiteral("elisa|dna |telia|saunalahti|moi |vodafone|three|o2 |broadband|laajakaista|"
                                // UK
                                "ee\\b|giffgaff|sky *(broadband|mobile)|virgin *media|bt *group|plusnet|talktalk")),
        makeRule(QStringLiteral("Health & Pharmacy"),
                 QStringLiteral("apteekki|pharmacy|terveystalo|mehilainen|pihlajalinna|hammas|dental|"
                                "laakari|doctor|hospital|sairaala|optikko|"
                                // UK
                                "boots|superdrug|lloyds *pharmacy|nhs\\b|specsavers|vision *express|bupa")),
        makeRule(QStringLiteral("Shopping"),
                 QStringLiteral("verkkokauppa|gigantti|power|clas *ohlson|ikea|jysk|zalando|hm\\b|h&m|"
                                "zara|uniqlo|amazon|ebay|aliexpress|temu|stockmann|sokos|prisma *verkko|"
                                "xxl|intersport|biltema|motonet|puuilo|tokmanni|"
                                // UK
                                "argos|john *lewis|currys|screwfix|b&q|wickes|homebase|wilko|primark|next *retail|asos|sports *direct|decathlon")),
        makeRule(QStringLiteral("Entertainment"),
                 QStringLiteral("finnkino|cinema|elokuva|teatteri|theatre|museo|museum|konsertti|"
                                "concert|ticketmaster|lippu\\.fi|tiketti|kuntosali|gym|elixia|fitness|"
                                // UK
                                "odeon|cineworld|vue *cinema|puregym|the *gym *group|nuffield *health|david *lloyd|national *trust|english *heritage")),
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

    const auto take = [&](const QString &category, const QString &expression) {
        if (category.isEmpty() || expression.isEmpty())
            return true;
        const Rule rule = makeRule(category, expression);
        if (!rule.pattern.isValid()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Invalid pattern for %1: %2")
                                    .arg(category, rule.pattern.errorString());
            }
            return false;
        }
        loaded.append(rule);
        return true;
    };

    if (root.value(QLatin1String("rules")).isArray()) {
        // Ordered form: the array is the precedence, first match wins.
        const QJsonArray rules = root.value(QLatin1String("rules")).toArray();
        for (const QJsonValue &entry : rules) {
            const QJsonObject object = entry.toObject();
            if (!take(object.value(QLatin1String("category")).toString(),
                      object.value(QLatin1String("pattern")).toString())) {
                return false;
            }
        }
    } else {
        /*
         * The older {"category": ["regex", ...]} form. Its order was never
         * meaningful -- QJsonObject hands keys back sorted -- so reading it
         * cannot recover an intent that was not stored. Saving afterwards
         * writes the ordered form, which at least stops the order shifting
         * again from then on.
         */
        for (auto it = root.constBegin(); it != root.constEnd(); ++it) {
            const QJsonArray patterns = it.value().toArray();
            for (const QJsonValue &pattern : patterns) {
                if (!take(it.key(), pattern.toString()))
                    return false;
            }
        }
    }

    setRules(loaded);
    return true;
}

bool Categoriser::saveToFile(const QString &path, QString *errorMessage) const
{
    QDir().mkpath(QFileInfo(path).absolutePath());

    // An array, not an object keyed by category: order is precedence here, and
    // a JSON object sorts its keys, so the previous format quietly rearranged
    // the rules every time the file was read back.
    QJsonArray rules;
    for (const Rule &rule : m_rules) {
        rules.append(QJsonObject{
            {QLatin1String("category"), rule.category},
            {QLatin1String("pattern"), rule.pattern.pattern()},
        });
    }
    const QJsonObject root{
        {QLatin1String("version"), 2},
        {QLatin1String("rules"), rules},
    };

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Cannot write %1: %2").arg(path, file.errorString());
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

void Categoriser::setRule(const Rule &rule)
{
    if (!rule.pattern.isValid() || rule.category.isEmpty())
        return;

    QList<Rule> rules = m_rules;
    const QString pattern = rule.pattern.pattern();
    rules.removeIf([&pattern](const Rule &existing) {
        return existing.pattern.pattern() == pattern;
    });
    rules.prepend(rule);
    setRules(rules);
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
