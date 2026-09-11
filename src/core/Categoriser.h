#pragma once

#include "core/Transaction.h"

#include <QList>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

/*!
 * Rule-based spend categorisation.
 *
 * Rules are ordered; the first whose pattern matches the merchant text wins.
 * The defaults are a starting point for Finnish/Nordic and pan-European
 * merchants -- users are expected to edit the JSON rule file, so the built-in
 * list deliberately stays short rather than trying to be exhaustive.
 */
class Categoriser
{
public:
    struct Rule
    {
        QString category;
        QRegularExpression pattern;
    };

    Categoriser();

    //! Replaces the rule set. Invalid patterns are skipped.
    void setRules(const QList<Rule> &rules);
    const QList<Rule> &rules() const { return m_rules; }

    //! Loads rules from a JSON file of {"category": ["regex", ...]} objects.
    bool loadFromFile(const QString &path, QString *errorMessage = nullptr);
    //! Writes the current rules back out, creating parent directories.
    bool saveToFile(const QString &path, QString *errorMessage = nullptr) const;

    //! Returns the category for \a transaction, or "Income"/"Uncategorised".
    QString categorise(const Transaction &transaction) const;

    static QList<Rule> defaultRules();
    static QString uncategorised() { return QStringLiteral("Uncategorised"); }
    static QString income() { return QStringLiteral("Income"); }

private:
    QList<Rule> m_rules;
};
