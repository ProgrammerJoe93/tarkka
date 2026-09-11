#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>

/*!
 * The bank accounts currently linked to the application.
 *
 * Populated from the sessions Enable Banking returns; this is what the Connect
 * screen lists so the user can see exactly which accounts the app can read.
 */
class AccountModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by AppController")

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    struct Account
    {
        QString uid;         //!< Enable Banking account uid used for API calls.
        QString sessionId;
        QString institution;
        QString name;
        QString iban;
        QString currency;
        QString product;
    };

    enum Role {
        UidRole = Qt::UserRole + 1,
        SessionIdRole,
        InstitutionRole,
        NameRole,
        IbanRole,
        CurrencyRole,
        ProductRole,
    };
    Q_ENUM(Role)

    explicit AccountModel(QObject *parent = nullptr);

    void setAccounts(const QList<Account> &accounts);
    const QList<Account> &accounts() const { return m_accounts; }
    void clear();

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void countChanged();

private:
    QList<Account> m_accounts;
};
