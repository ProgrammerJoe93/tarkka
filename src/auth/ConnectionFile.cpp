#include "auth/ConnectionFile.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

// A marker rather than a bare object, so pointing the import at some other
// JSON file is refused instead of silently clearing the application id.
constexpr auto kMarkerKey = "tarkka";
constexpr auto kMarkerValue = "connection";
constexpr auto kVersionKey = "version";
constexpr int kVersion = 1;

constexpr auto kApplicationIdKey = "applicationId";
constexpr auto kCountryKey = "country";
constexpr auto kSessionIdsKey = "sessionIds";

} // namespace

QByteArray ConnectionFile::serialise(const Contents &contents)
{
    QJsonArray sessions;
    for (const QString &sessionId : contents.sessionIds)
        sessions.append(sessionId);

    const QJsonObject root{
        {QLatin1String(kMarkerKey), QLatin1String(kMarkerValue)},
        {QLatin1String(kVersionKey), kVersion},
        {QLatin1String(kApplicationIdKey), contents.applicationId},
        {QLatin1String(kCountryKey), contents.country},
        {QLatin1String(kSessionIdsKey), sessions},
    };
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

ConnectionFile::Contents ConnectionFile::parse(const QByteArray &json, QString *errorMessage)
{
    const auto fail = [errorMessage](const QString &message) {
        if (errorMessage)
            *errorMessage = message;
        return Contents{};
    };

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return fail(QStringLiteral("Not a readable file: %1").arg(parseError.errorString()));
    if (!document.isObject())
        return fail(QStringLiteral("This file does not contain bank connections."));

    const QJsonObject root = document.object();
    if (root.value(QLatin1String(kMarkerKey)).toString() != QLatin1String(kMarkerValue))
        return fail(QStringLiteral("This file does not contain bank connections."));

    const int version = root.value(QLatin1String(kVersionKey)).toInt();
    if (version > kVersion) {
        return fail(QStringLiteral("This file was written by a newer version of Tarkka "
                                   "(format %1) and cannot be read here.")
                        .arg(version));
    }

    Contents contents;
    contents.applicationId = root.value(QLatin1String(kApplicationIdKey)).toString().trimmed();
    contents.country = root.value(QLatin1String(kCountryKey)).toString().trimmed().toUpper();
    const QJsonArray sessions = root.value(QLatin1String(kSessionIdsKey)).toArray();
    for (const QJsonValue &session : sessions) {
        const QString sessionId = session.toString().trimmed();
        if (!sessionId.isEmpty())
            contents.sessionIds.append(sessionId);
    }

    if (!contents.isValid())
        return fail(QStringLiteral("This file names no application id, so there is nothing "
                                   "to connect with."));
    return contents;
}
