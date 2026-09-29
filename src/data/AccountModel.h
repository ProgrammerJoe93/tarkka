#pragma once

#include <QAbstractListModel>
#include <QDate>
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
        /*! IBAN, or for a credit card the masked PAN the bank reports instead. */
        QString identifier;
        QString currency;
        QString product;
        //! ISO 20022 cash account type: CACC, CARD, SVGS, LOAN, CASH, OTHR.
        QString cashAccountType;

        /*!
         * The balance the bank last reported, if it reported one.
         *
         * Not every bank serves balances, and a fetch can fail on its own
         * without the account being any less usable, so this is optional
         * rather than assumed. \c balanceName carries the bank's own label so
         * the UI can say which balance it is showing -- banks mean different
         * things by the word.
         */
        bool hasBalance = false;
        qint64 balanceMinor = 0;
        QString balanceCurrency;
        QString balanceName;
        QString balanceType;
        QDate balanceDate;

        //! A credit card is a card account, not a payment account with an IBAN.
        bool isCard() const
        {
            return cashAccountType.compare(QLatin1String("CARD"), Qt::CaseInsensitive) == 0;
        }
    };

    enum Role {
        UidRole = Qt::UserRole + 1,
        SessionIdRole,
        InstitutionRole,
        NameRole,
        IdentifierRole,
        CurrencyRole,
        ProductRole,
        CashAccountTypeRole,
        IsCardRole,
        HasBalanceRole,
        BalanceTextRole,
        BalanceMinorRole,
        //! The bank's own label for the balance, e.g. "Available balance".
        BalanceNameRole,
    };
    Q_ENUM(Role)

    explicit AccountModel(QObject *parent = nullptr);

    void setAccounts(const QList<Account> &accounts);
    /*!
     * Attaches a freshly fetched balance to the account with \a uid.
     *
     * Separate from setAccounts() because balances arrive one request per
     * account, after the accounts themselves are already on screen.
     */
    void setBalance(const QString &uid, const Account &balance);
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
