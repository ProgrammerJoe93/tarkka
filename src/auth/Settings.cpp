#include "auth/Settings.h"

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>

namespace {

constexpr auto kApplicationId = "enablebanking/applicationId";
constexpr auto kPrivateKeyPath = "enablebanking/privateKeyPath";
constexpr auto kRedirectPort = "enablebanking/redirectPort";
constexpr auto kApiBaseUrl = "enablebanking/apiBaseUrl";
constexpr auto kCountry = "enablebanking/country";
constexpr auto kSessionIds = "enablebanking/sessionIds";

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

QString Settings::databasePath()
{
    return dataDir() + QStringLiteral("/transactions.sqlite");
}

QString Settings::categoryRulesPath()
{
    return dataDir() + QStringLiteral("/categories.json");
}
