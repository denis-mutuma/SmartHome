#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>


int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    app.setOrganizationName("Mutuma");
    app.setOrganizationDomain("https://github.com/denis-mutuma");
    app.setApplicationName("SmartHome");
    app.setApplicationVersion("1.0.0");
    app.setWindowIcon(QIcon(QStringLiteral(":/qt/qml/SmartHome/assets/images/mutuma.jpg")));
    app.setApplicationDisplayName("SmartHome");

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("SmartHome", "Main");

    return app.exec();
}
