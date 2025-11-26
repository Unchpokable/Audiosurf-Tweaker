#include "precompiled.hxx"

#include "qmetacompat.hxx"

#include "Core/logging.hxx"

#include "Backend/backgrounds_provider.hxx"
#include "Backend/engine.hxx"
#include "Backend/icons_provider.hxx"

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

    auto tweaker_engine = std::make_unique<Engine>();

    engine.addImageProvider("icons", new IconsProvider());
    engine.addImageProvider("backgrounds", new BackgroundsProvider(tweaker_engine.get()));

    engine.loadFromModule("QTweaker", "Main");

    return app.exec();
}
