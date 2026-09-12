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

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Tarkka", "Main");

    return app.exec();
}
