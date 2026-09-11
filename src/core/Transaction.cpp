#include "core/Transaction.h"

#include <QCryptographicHash>

QString makeTransactionId(const QString &source,
                          const QString &accountId,
                          const QString &bankReference,
                          const QDate &date,
                          qint64 amountMinor,
                          const QString &description,
                          int ordinal)
{
    if (!bankReference.isEmpty())
        return source + QLatin1Char(':') + accountId + QLatin1Char(':') + bankReference;

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(source.toUtf8());
    hash.addData(accountId.toUtf8());
    hash.addData(date.toString(Qt::ISODate).toUtf8());
    hash.addData(QByteArray::number(amountMinor));
    hash.addData(description.trimmed().simplified().toUtf8());
    hash.addData(QByteArray::number(ordinal));
    return source + QLatin1Char(':') + accountId + QLatin1Char(':')
           + QString::fromLatin1(hash.result().toHex().left(24));
}
