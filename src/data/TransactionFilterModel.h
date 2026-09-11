#pragma once

#include <QQmlEngine>
#include <QSortFilterProxyModel>

/*!
 * Live text filtering over TransactionModel.
 *
 * Date, bank and category narrowing happen in SQL; only the free-text search
 * lives here so that typing filters instantly without a database round trip per
 * keystroke.
 */
class TransactionFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by AppController")

    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    explicit TransactionFilterModel(QObject *parent = nullptr);

    QString searchText() const { return m_searchText; }
    void setSearchText(const QString &text);

signals:
    void searchTextChanged();
    void countChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString m_searchText;
};
