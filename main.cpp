#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QIcon>


int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    app.setOrganizationName("Mutuma");
    app.setOrganizationDomain("https://github.com/denis-mutuma");
    app.setApplicationName("SmartHome");
    app.setApplicationVersion("1.0.0");
    app.setWindowIcon(QIcon("assets/images/mutuma.jpg"));
    app.setApplicationDisplayName("SmartHome");

    QQmlApplicationEngine engine;
    const QUrl url(u"qrc:/SmartHome/Main.qml"_qs);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
