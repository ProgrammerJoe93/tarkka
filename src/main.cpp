#include "auth/Settings.h"
#include "i18n/Translation.h"

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QCoreApplication::setOrganizationName(QStringLiteral("tarkka"));
    QCoreApplication::setApplicationName(QStringLiteral("Tarkka"));
    QCoreApplication::setApplicationVersion(QStringLiteral(TARKKA_VERSION));

    // The app ships its own palette, so pin the style rather than inheriting a
    // desktop theme that would fight it.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    // Before the engine loads anything: the first frame should already be in
    // the right language rather than showing English and correcting itself.
    // Needs the organisation and application names above to find the setting.
    Translation::apply(Settings().language());

    // Qt picks the closest size from the set for title bars, task switchers
    // and the like.
    QIcon icon;
    for (int size : {16, 24, 32, 48, 64, 128, 256})
        icon.addFile(QStringLiteral(":/icons/tarkka-%1.png").arg(size));
    QGuiApplication::setWindowIcon(icon);

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Tarkka", "Main");

    return app.exec();
}
