#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QtGlobal>

/*!
 * A monthly spending target per category.
 *
 * Kept beside the category rules rather than in the database: like those rules
 * it is a statement of intent rather than a record of what happened, and a user
 * who wants to read or edit it by hand should be able to.
 *
 * Targets are monthly and in minor units, so they compare directly with the
 * figures the dashboard already computes. A category with no target is not the
 * same as one budgeted at zero -- the first is unbudgeted, the second is a
 * decision to spend nothing -- so absence is distinct from 0 throughout.
 */
class Budgets
{
public:
    //! The target for \a category, or -1 when none is set.
    qint64 target(const QString &category) const;
    bool has(const QString &category) const { return m_targets.contains(category); }
    //! A negative \a minor clears the target rather than storing nonsense.
    void setTarget(const QString &category, qint64 minor);

    //! Every category carrying a target, in no particular order.
    QStringList categories() const { return m_targets.keys(); }
    bool isEmpty() const { return m_targets.isEmpty(); }
    const QHash<QString, qint64> &all() const { return m_targets; }

    //! Reads {"category": minorUnits} objects. Missing file is not an error.
    bool loadFromFile(const QString &path, QString *errorMessage = nullptr);
    bool saveToFile(const QString &path, QString *errorMessage = nullptr) const;

private:
    QHash<QString, qint64> m_targets;
};
