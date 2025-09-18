#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include "skin_item.hxx"

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);

    qRegisterMetaType<QImageList>();

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
