#include "auth/Settings.h"
#include "i18n/Translation.h"

#include <QCoreApplication>
#include <QSettings>
#include <QStandardPaths>
#include <QTest>

class TestSettings : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void defaultsToFollowingTheSystem();
    void roundTripsEachPalette();
    void normalisesCaseAndWhitespace();
    void fallsBackToSystemOnAnUnknownMode();
    void readsAnUnknownStoredValueAsSystem();

    void languageDefaultsToFollowingTheSystem();
    void languageRoundTripsEachSupportedCode();
    void languageFallsBackToSystemOnAnUnknownCode();
    void resolvesAnExplicitCodeToItself();
    void resolvesSystemToASupportedLanguage();
    void appliesEnglishWithoutACatalogue();
};

void TestSettings::initTestCase()
{
    // Keeps the suite out of the real config file; QSettings honours the test
    // mode the same way QStandardPaths does.
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("tarkka-tests"));
    QCoreApplication::setApplicationName(QStringLiteral("tarkka-tests"));
}

void TestSettings::init()
{
    QSettings().clear();
}

void TestSettings::defaultsToFollowingTheSystem()
{
    QCOMPARE(Settings().appearance(), QStringLiteral("system"));
}

void TestSettings::roundTripsEachPalette()
{
    Settings settings;
    for (const QString &mode : {QStringLiteral("light"), QStringLiteral("dark"),
                                QStringLiteral("system")}) {
        settings.setAppearance(mode);
        QCOMPARE(settings.appearance(), mode);
    }
}

void TestSettings::normalisesCaseAndWhitespace()
{
    Settings settings;
    settings.setAppearance(QStringLiteral("  Dark "));
    QCOMPARE(settings.appearance(), QStringLiteral("dark"));
}

void TestSettings::fallsBackToSystemOnAnUnknownMode()
{
    Settings settings;
    settings.setAppearance(QStringLiteral("dark"));
    settings.setAppearance(QStringLiteral("solarized"));
    QCOMPARE(settings.appearance(), QStringLiteral("system"));
}

void TestSettings::readsAnUnknownStoredValueAsSystem()
{
    QSettings().setValue(QStringLiteral("ui/appearance"), QStringLiteral("chartreuse"));
    QCOMPARE(Settings().appearance(), QStringLiteral("system"));
}

void TestSettings::languageDefaultsToFollowingTheSystem()
{
    QCOMPARE(Settings().language(), QStringLiteral("system"));
}

void TestSettings::languageRoundTripsEachSupportedCode()
{
    Settings settings;
    for (const QString &code : Translation::supported()) {
        settings.setLanguage(code);
        QCOMPARE(settings.language(), code);
    }
}

void TestSettings::languageFallsBackToSystemOnAnUnknownCode()
{
    Settings settings;
    settings.setLanguage(QStringLiteral("fi"));
    // A code from a build that shipped more catalogues than this one.
    settings.setLanguage(QStringLiteral("sv"));
    QCOMPARE(settings.language(), QStringLiteral("system"));
}

void TestSettings::resolvesAnExplicitCodeToItself()
{
    for (const QString &code : Translation::supported())
        QCOMPARE(Translation::resolve(code), code);
}

void TestSettings::resolvesSystemToASupportedLanguage()
{
    // Whatever the machine running the suite prefers, "system" has to name a
    // language there is actually a catalogue for.
    QVERIFY(Translation::supported().contains(Translation::resolve(QStringLiteral("system"))));
}

void TestSettings::appliesEnglishWithoutACatalogue()
{
    // English is the source language, so there is no tarkka_en.qm to find;
    // apply() must still report English rather than failing into it.
    QCOMPARE(Translation::apply(QStringLiteral("en")), QStringLiteral("en"));
}

QTEST_GUILESS_MAIN(TestSettings)
#include "tst_settings.moc"
