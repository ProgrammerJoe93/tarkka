#pragma once

#include "core/Transaction.h"

#include <QDate>
#include <QObject>
#include <QString>

/*!
 * The date window to ask a provider for, given the range the UI is showing.
 *
 * Widened backwards so rows a bank books late still land in the month they
 * belong to, and clamped forwards to \a today: a bank has no transactions
 * dated after today, and several reject a future date_to outright rather than
 * returning nothing -- S-Pankki answers HTTP 422 "Wrong transaction period
 * requested" where Nordea and Revolut simply ignore the surplus. The visible
 * range is a whole calendar month by default, so for most of every month the
 * naive end date is in the future.
 */
struct SyncWindow
{
    QDate from;
    QDate to;

    static SyncWindow forRange(const QDate &rangeFrom, const QDate &rangeTo, const QDate &today)
    {
        SyncWindow window;
        window.from = rangeFrom.isValid() ? rangeFrom.addDays(-7) : QDate();
        if (rangeTo.isValid())
            window.to = qMin(rangeTo.addDays(1), today);
        return window;
    }

    bool isValid() const { return from.isValid() && to.isValid() && from <= to; }

    /*!
     * A narrower window to retry with after a bank refused the period.
     *
     * Banks cap how far back they will serve once the initial consent is a few
     * days old -- ninety days is the usual figure -- and answer 422 for
     * anything older rather than returning the part they do have. Retrying
     * from inside that cap saves the account's recent rows instead of losing
     * them along with the old ones.
     *
     * Returns an invalid window when there is nothing narrower worth asking
     * for: either the request was already inside the cap, so the refusal was
     * about something else, or the whole window predates it and the bank has
     * nothing to give.
     */
    static SyncWindow narrowed(const SyncWindow &window, const QDate &today, int historyDays = 90)
    {
        if (!window.isValid())
            return {};
        const QDate floor = today.addDays(-historyDays);
        if (window.from >= floor || floor > window.to)
            return {};
        return SyncWindow{floor, window.to};
    }
};

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
