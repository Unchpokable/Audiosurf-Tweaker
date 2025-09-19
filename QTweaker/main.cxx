#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include "Backend/skin_item.hxx"

#include "qmetacompat.hxx"

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);

    qt::dirty::compat_init();

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
