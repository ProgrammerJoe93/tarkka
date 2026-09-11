#pragma once

#include "core/Transaction.h"

#include <QDate>
#include <QObject>
#include <QString>

/*!
 * Common interface for anything that can produce transactions.
 *
 * Providers are asynchronous and report through signals: a live API provider
 * needs several round trips, a CSV import completes immediately, and the UI
 * treats both the same way.
 */
class Provider : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
    ~Provider() override = default;

    //! Stable identifier stored on every row this provider emits.
    virtual QString sourceId() const = 0;
    //! Human-readable name for the UI.
    virtual QString displayName() const = 0;

signals:
    //! A batch of normalised rows. May fire more than once per fetch.
    void transactionsReady(const TransactionList &transactions);
    //! The fetch finished; \a summary is a short line for the status bar.
    void finished(const QString &summary);
    void failed(const QString &message);
    void progress(const QString &message);
};
