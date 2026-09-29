#pragma once

#include "core/Recurring.h"

#include <QAbstractListModel>
#include <QQmlEngine>

/*!
 * The standing payments found in the rows currently loaded.
 *
 * Committed spending, which is the part of a budget that cannot be flexed and
 * the part most likely to have been forgotten about.
 */
class RecurringModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by AppController")

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    //! Everything here added together: the monthly commitment.
    Q_PROPERTY(QString totalText READ totalText NOTIFY countChanged)

public:
    enum Role {
        MerchantRole = Qt::UserRole + 1,
        CategoryRole,
        TypicalTextRole,
        LastSeenTextRole,
        OccurrencesRole,
        /*!
         * Whether the newest charge differs from the usual one.
         *
         * The reason the latest amount is tracked separately: a price that has
         * crept up is the thing worth noticing, and an average would hide it.
         */
        PriceChangedRole,
        LatestTextRole,
    };
    Q_ENUM(Role)

    explicit RecurringModel(QObject *parent = nullptr);

    void setPayments(const QList<Recurring::Payment> &payments, const QString &currency);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString totalText() const;

signals:
    void countChanged();

private:
    QList<Recurring::Payment> m_payments;
    QString m_currency;
};
