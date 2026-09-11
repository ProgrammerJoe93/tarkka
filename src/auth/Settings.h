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

    //! Session ids returned by POST /sessions, one per connected bank.
    QStringList sessionIds() const;
    void addSessionId(const QString &sessionId);
    void removeSessionId(const QString &sessionId);
    void clearSessionIds();

    //! Reads the PEM key from disk. Empty on failure, with \a errorMessage set.
    QByteArray readPrivateKey(QString *errorMessage = nullptr) const;

    bool isConfigured() const;

    static QString databasePath();
    static QString categoryRulesPath();
};
