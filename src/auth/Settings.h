#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

/*!
 * Persistent application configuration.
 *
 * Credentials are deliberately split: the Enable Banking application id and the
 * *path* to the private key live in QSettings, while the key itself stays where
 * the user put it and is read on demand. Nothing secret is copied into the
 * config file.
 */
class Settings
{
public:
    Settings();

    QString applicationId() const;
    void setApplicationId(const QString &id);

    QString privateKeyPath() const;
    void setPrivateKeyPath(const QString &path);

    quint16 redirectPort() const;
    void setRedirectPort(quint16 port);

    QString apiBaseUrl() const;
    void setApiBaseUrl(const QString &url);

    QString country() const;
    void setCountry(const QString &code);

    /*!
     * Which palette the UI draws with: "system", "light" or "dark".
     *
     * "system" defers to the platform's colour scheme and is the default.
     * Both accessors normalise anything else to "system", so a hand-edited
     * config file cannot leave the UI without a palette to fall back on.
     */
    QString appearance() const;
    void setAppearance(const QString &mode);

    /*!
     * Which language the UI is in: "system", or a code from
     * Translation::supported().
     *
     * Normalised the same way as appearance(), and for the same reason: the
     * value reaches a catalogue lookup, and an unrecognised one would leave
     * the UI with no strings rather than the wrong ones.
     */
    QString language() const;
    void setLanguage(const QString &language);

    //! Session ids returned by POST /sessions, one per connected bank.
    QStringList sessionIds() const;
    void addSessionId(const QString &sessionId);
    void removeSessionId(const QString &sessionId);
    void clearSessionIds();

    //! Reads the PEM key from disk. Empty on failure, with \a errorMessage set.
    QByteArray readPrivateKey(QString *errorMessage = nullptr) const;

    /*!
     * Trading 212 credentials.
     *
     * The API authenticates with HTTP Basic, taking the API key as the username
     * and the API secret as the password. Both are secrets the user pastes in
     * rather than files they already manage, so the pair is written to its own
     * owner-only file instead of being left in the plain-text settings.
     */
    QString trading212ApiKey() const;
    QString trading212ApiSecret() const;
    bool setTrading212Credentials(const QString &key,
                                  const QString &secret,
                                  QString *errorMessage = nullptr);
    bool hasTrading212Credentials() const;
    QString trading212BaseUrl() const;
    void setTrading212BaseUrl(const QString &url);

    bool isConfigured() const;

    static QString databasePath();
    static QString categoryRulesPath();
};
