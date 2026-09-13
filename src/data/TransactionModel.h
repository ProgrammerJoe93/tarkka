#pragma once

#include "core/Transaction.h"

#include <QAbstractListModel>
#include <QQmlEngine>

/*!
 * Exposes a list of transactions to QML.
 *
 * The model is filled wholesale from TransactionStore after each query rather
 * than tracking incremental changes -- transaction counts here are in the
 * thousands at most, and a full reset keeps the data path easy to reason about.
 */
class TransactionModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by AppController")

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        InstitutionRole,
        AccountNameRole,
        DateRole,
        DateTextRole,
        DescriptionRole,
        CounterpartyRole,
        ReferenceRole,
        AmountMinorRole,
        AmountTextRole,
        CurrencyRole,
        CategoryRole,
        IsSpendRole,
        IsPendingRole,
        IsExcludedRole,
    };
    Q_ENUM(Role)

    explicit TransactionModel(QObject *parent = nullptr);

    void setTransactions(const TransactionList &transactions);
    const TransactionList &transactions() const { return m_transactions; }

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void countChanged();

private:
    TransactionList m_transactions;
};
