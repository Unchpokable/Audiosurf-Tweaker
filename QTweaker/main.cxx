#include "precompiled.hxx"

#include "qmetacompat.hxx"

#include "Core/logging.hxx"

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);

    logging::add_sink(logging::sinks::stderr_ansi);
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
