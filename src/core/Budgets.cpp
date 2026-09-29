#include "core/Budgets.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

qint64 Budgets::target(const QString &category) const
{
    return m_targets.value(category, -1);
}

void Budgets::setTarget(const QString &category, qint64 minor)
{
    if (category.isEmpty())
        return;
    if (minor < 0) {
        m_targets.remove(category);
        return;
    }
    m_targets.insert(category, minor);
}

bool Budgets::loadFromFile(const QString &path, QString *errorMessage)
{
    QFile file(path);
    if (!file.exists()) {
        // Nobody has set a budget yet, which is the starting state rather than
        // a problem to report.
        m_targets.clear();
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Cannot read %1: %2").arg(path, file.errorString());
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("%1 is not a readable budget file: %2")
                                .arg(path, parseError.errorString());
        }
        return false;
    }

    m_targets.clear();
    const QJsonObject root = document.object();
    for (auto it = root.constBegin(); it != root.constEnd(); ++it) {
        const qint64 minor = static_cast<qint64>(it.value().toDouble(-1));
        if (minor >= 0)
            m_targets.insert(it.key(), minor);
    }
    return true;
}

bool Budgets::saveToFile(const QString &path, QString *errorMessage) const
{
    QDir().mkpath(QFileInfo(path).absolutePath());

    QJsonObject root;
    for (auto it = m_targets.constBegin(); it != m_targets.constEnd(); ++it)
        root.insert(it.key(), static_cast<double>(it.value()));

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Cannot write %1: %2").arg(path, file.errorString());
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}
