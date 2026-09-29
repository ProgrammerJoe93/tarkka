#include "core/Balance.h"

QStringList Balance::preferenceOrder()
{
    /*
     * ITAV first: the interim available balance already accounts for card
     * authorisations that have not settled, so it is the closest thing to
     * money you could actually spend. CLAV is the same idea at close of
     * business. Only then the booked balances, which ignore anything pending
     * and so read high on the day you have used the card.
     */
    return {
        QStringLiteral("ITAV"),  // Interim available
        QStringLiteral("CLAV"),  // Closing available
        QStringLiteral("XPCD"),  // Expected
        QStringLiteral("ITBD"),  // Interim booked
        QStringLiteral("CLBD"),  // Closing booked
        QStringLiteral("PRCD"),  // Previously closed booked
        QStringLiteral("OPAV"),  // Opening available
        QStringLiteral("OPBD"),  // Opening booked
    };
}

Balance::Reported Balance::preferred(const QList<Reported> &reported)
{
    if (reported.isEmpty())
        return {};

    for (const QString &wanted : preferenceOrder()) {
        for (const Reported &candidate : reported) {
            if (candidate.type.compare(wanted, Qt::CaseInsensitive) == 0)
                return candidate;
        }
    }

    // An unrecognised code from a bank that reports something non-standard is
    // still a balance, and showing it beats showing nothing.
    return reported.constFirst();
}
