#include "auth/Settings.h"

#include "i18n/Translation.h"

#include <QDir>
#include <QFile>
#include <QPair>
#include <QSettings>
#include <QStandardPaths>

namespace {

constexpr auto kApplicationId = "enablebanking/applicationId";
constexpr auto kPrivateKeyPath = "enablebanking/privateKeyPath";
constexpr auto kRedirectPort = "enablebanking/redirectPort";
constexpr auto kApiBaseUrl = "enablebanking/apiBaseUrl";
constexpr auto kCountry = "enablebanking/country";
constexpr auto kAppearance = "ui/appearance";
constexpr auto kLanguage = "ui/language";
constexpr auto kSessionIds = "enablebanking/sessionIds";
constexpr auto kT212BaseUrl = "trading212/baseUrl";

/*!
 * Reads a setting that may only hold one of \a allowed.
 *
 * Anything else -- a value from a newer build, or a hand-edited config file --
 * reads back as the first entry, which is "system" for both of these: the UI
 * has to render with something, and deferring to the platform is the safest
 * something.
 */
QString choice(const char *key, const QStringList &allowed)
{
    const QString value = QSettings().value(QLatin1String(key)).toString();
    return allowed.contains(value) ? value : allowed.constFirst();
}

//! Writes a setting that may only hold one of \a allowed, normalising the rest.
void setChoice(const char *key, const QStringList &allowed, const QString &value)
{
    const QString normalised = value.trimmed().toLower();
    QSettings().setValue(QLatin1String(key),
                         allowed.contains(normalised) ? normalised : allowed.constFirst());
}

QStringList appearanceModes()
{
    return {QStringLiteral("system"), QStringLiteral("light"), QStringLiteral("dark")};
}

QStringList languageChoices()
{
    return QStringList{QStringLiteral("system")} + Translation::supported();
}

QString dataDir()
{
    const QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(path);
    return path;
}

} // namespace

Settings::Settings() = default;

QString Settings::applicationId() const
{
    return QSettings().value(QLatin1String(kApplicationId)).toString();
}

void Settings::setApplicationId(const QString &id)
{
    QSettings().setValue(QLatin1String(kApplicationId), id.trimmed());
}

QString Settings::privateKeyPath() const
{
    return QSettings().value(QLatin1String(kPrivateKeyPath)).toString();
}

void Settings::setPrivateKeyPath(const QString &path)
{
    QSettings().setValue(QLatin1String(kPrivateKeyPath), path.trimmed());
}

quint16 Settings::redirectPort() const
{
    return static_cast<quint16>(
        QSettings().value(QLatin1String(kRedirectPort), 8765).toUInt());
}

void Settings::setRedirectPort(quint16 port)
{
    QSettings().setValue(QLatin1String(kRedirectPort), port);
}

QString Settings::apiBaseUrl() const
{
    return QSettings()
        .value(QLatin1String(kApiBaseUrl), QStringLiteral("https://api.enablebanking.com"))
        .toString();
}

void Settings::setApiBaseUrl(const QString &url)
{
    QSettings().setValue(QLatin1String(kApiBaseUrl), url.trimmed());
}

QString Settings::country() const
{
    return QSettings().value(QLatin1String(kCountry), QStringLiteral("FI")).toString();
}

void Settings::setCountry(const QString &code)
{
    QSettings().setValue(QLatin1String(kCountry), code.trimmed().toUpper());
}

QString Settings::appearance() const
{
    return choice(kAppearance, appearanceModes());
}

void Settings::setAppearance(const QString &mode)
{
    setChoice(kAppearance, appearanceModes(), mode);
}

QString Settings::language() const
{
    return choice(kLanguage, languageChoices());
}

void Settings::setLanguage(const QString &language)
{
    setChoice(kLanguage, languageChoices(), language);
}

QStringList Settings::sessionIds() const
{
    return QSettings().value(QLatin1String(kSessionIds)).toStringList();
}

void Settings::addSessionId(const QString &sessionId)
{
    if (sessionId.isEmpty())
        return;
    QStringList ids = sessionIds();
    if (ids.contains(sessionId))
        return;
    ids.append(sessionId);
    QSettings().setValue(QLatin1String(kSessionIds), ids);
}

void Settings::removeSessionId(const QString &sessionId)
{
    QStringList ids = sessionIds();
    if (ids.removeAll(sessionId) > 0)
        QSettings().setValue(QLatin1String(kSessionIds), ids);
}

void Settings::clearSessionIds()
{
    QSettings().remove(QLatin1String(kSessionIds));
}

QByteArray Settings::readPrivateKey(QString *errorMessage) const
{
    const QString path = privateKeyPath();
    if (path.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("No private key file has been configured.");
        return {};
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Cannot read the private key at %1: %2")
                                .arg(path, file.errorString());
        return {};
    }
    return file.readAll();
}

bool Settings::isConfigured() const
{
    return !applicationId().isEmpty() && !privateKeyPath().isEmpty();
}

namespace {

QString trading212KeyPath()
{
    return dataDir() + QStringLiteral("/trading212.key");
}

//! Reads the credential file as {key, secret}; either may be empty.
QPair<QString, QString> readTrading212File()
{
    QFile file(trading212KeyPath());
    if (!file.open(QIODevice::ReadOnly))
        return {};

    const QStringList lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
    const QString key = lines.value(0).trimmed();
    const QString secret = lines.value(1).trimmed();
    return {key, secret};
}

} // namespace

QString Settings::trading212ApiKey() const
{
    return readTrading212File().first;
}

QString Settings::trading212ApiSecret() const
{
    return readTrading212File().second;
}

bool Settings::hasTrading212Credentials() const
{
    const auto credentials = readTrading212File();
    return !credentials.first.isEmpty() && !credentials.second.isEmpty();
}

bool Settings::setTrading212Credentials(const QString &key,
                                        const QString &secret,
                                        QString *errorMessage)
{
    const QString path = trading212KeyPath();

    if (key.trimmed().isEmpty() && secret.trimmed().isEmpty()) {
        QFile::remove(path);
        return true;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Cannot store the Trading 212 credentials in %1: %2")
                                .arg(path, file.errorString());
        return false;
    }
    // Narrow the permissions before the secrets are written, not after.
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    file.write(key.trimmed().toUtf8() + '\n' + secret.trimmed().toUtf8() + '\n');
    return true;
}

QString Settings::trading212BaseUrl() const
{
    return QSettings()
        .value(QLatin1String(kT212BaseUrl), QStringLiteral("https://live.trading212.com"))
        .toString();
}

void Settings::setTrading212BaseUrl(const QString &url)
{
    QSettings().setValue(QLatin1String(kT212BaseUrl), url.trimmed());
}

QString Settings::databasePath()
{
    return dataDir() + QStringLiteral("/transactions.sqlite");
}

QString Settings::categoryRulesPath()
{
    return dataDir() + QStringLiteral("/categories.json");
}
