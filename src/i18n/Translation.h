#pragma once

#include <QString>
#include <QStringList>

/*!
 * The interface languages, and the switching between them.
 *
 * English is the source language rather than a translation of anything, so it
 * has no catalogue: selecting it means removing whatever translator is
 * installed instead of loading one.
 */
namespace Translation {

//! Language codes the UI can be shown in, English first, in picker order.
QStringList supported();

/*!
 * Turns a stored setting into a language code.
 *
 * \a language is either a code from supported() or "system", which consults
 * the platform's ordered list of preferred languages.
 */
QString resolve(const QString &language);

/*!
 * Installs the catalogue for \a language, which may be "system", and points
 * the default QLocale at it so that month names and decimal separators follow
 * the same choice the strings do.
 *
 * Returns the code actually in effect. A catalogue that will not load falls
 * back to English rather than leaving half the UI in each language.
 *
 * QML strings already on screen keep the language they were built with until
 * QQmlEngine::retranslate() runs, so callers changing the language after
 * startup have to do that too.
 */
QString apply(const QString &language);

} // namespace Translation
