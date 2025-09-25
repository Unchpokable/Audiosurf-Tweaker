#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

#include "qmetacompat.hxx"

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);

    qt::dirty::compat_init();

    QQuickStyle::setStyle("Material");

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() {
            QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);
    engine.loadFromModule("QTweaker", "Main");

    return app.exec();
}
