#include "i18n/Translation.h"

#include <QCoreApplication>
#include <QLocale>
#include <QTranslator>

namespace {

constexpr auto kSourceLanguage = "en";

/*!
 * The one translator the app installs.
 *
 * Reused rather than allocated per switch: installing a second would leave the
 * first in the chain, and Qt would keep answering from whichever matched
 * first.
 */
QTranslator &translator()
{
    static QTranslator instance;
    return instance;
}

/*!
 * The locale to format dates and amounts with while the UI speaks \a code.
 *
 * Where the language already matches the platform's, the platform's region is
 * kept: an en_GB user asking for English wants 31/12, not the 12/31 a bare
 * "en" would hand them. A language the platform is not in brings its own.
 *
 * Rebuilt from language and territory rather than handing back
 * QLocale::system() itself, which is not one locale but several: it takes its
 * name from LANG and its month names from LC_TIME, so a Finnish desktop with
 * LC_TIME left on English reports fi_FI and then spells the month
 * "September". Qt's own CLDR data has no such seam.
 */
QLocale localeFor(const QString &code)
{
    const QLocale wanted(code);
    const QLocale system = QLocale::system();
    if (wanted.language() != system.language())
        return wanted;
    return QLocale(system.language(), system.territory());
}

} // namespace

QStringList Translation::supported()
{
    return {QString::fromLatin1(kSourceLanguage), QStringLiteral("fi")};
}

QString Translation::resolve(const QString &language)
{
    if (supported().contains(language))
        return language;

    // uiLanguages is the platform's ordered preference list, so someone whose
    // first choice this app has no catalogue for still gets their second
    // before falling back to English. Entries are tags like "fi-FI"; only the
    // language subtag is matched, because the catalogues are not regional.
    const QStringList preferred = QLocale::system().uiLanguages();
    for (const QString &tag : preferred) {
        const QString code = tag.section(QLatin1Char('-'), 0, 0).toLower();
        if (supported().contains(code))
            return code;
    }
    return QString::fromLatin1(kSourceLanguage);
}

QString Translation::apply(const QString &language)
{
    QString code = resolve(language);

    QCoreApplication::removeTranslator(&translator());
    if (code != QLatin1String(kSourceLanguage)) {
        if (translator().load(QStringLiteral(":/i18n/tarkka_%1").arg(code)))
            QCoreApplication::installTranslator(&translator());
        else
            code = QString::fromLatin1(kSourceLanguage);
    }

    QLocale::setDefault(localeFor(code));
    return code;
}
