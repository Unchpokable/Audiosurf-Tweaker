#include "precompiled.hxx"

#include "engine.hxx"

#include "pack_data.hxx"
#include "packager.hxx"

Engine::Engine(QObject* parent) : QObject(parent)
{
    m_skin_changer = new SkinChangerBackend(this);
    m_tweaker = new TweakerBackend(this);
    m_color_configurator = new ColorConfiguratorBackend(this);
    m_settings = new AppSettingsBackend(this);

    connect(&m_loading_watcher, &LoadingWatcher::progressRangeChanged, this, &Engine::loading_range_updated);
    connect(&m_loading_watcher, &LoadingWatcher::progressValueChanged, this, &Engine::loading_value_changed);
    connect(&m_loading_watcher, &LoadingWatcher::finished, this, &Engine::on_loading_completed);
}

SkinChangerBackend* Engine::skin_changer()
{
    return m_skin_changer;
}

TweakerBackend* Engine::tweaker()
{
    return m_tweaker;
}

ColorConfiguratorBackend* Engine::color_configurator()
{
    return m_color_configurator;
}

Q_INVOKABLE AppSettingsBackend* Engine::settings()
{
    return m_settings;
}

Q_INVOKABLE void Engine::startup()
{
    static auto local_skins_folder = "Skins";

    QFileInfo existing_info(local_skins_folder);

    if(!existing_info.exists()) {
        emit startup_failed("No skins folder found");
    }

    QStringList found_skins;

    QDirIterator dir_iterator(local_skins_folder, QStringList() << core::file_extension, QDir::Files);

    while(dir_iterator.hasNext()) {
        auto next = dir_iterator.next();
        found_skins.append(QFileInfo(next).fileName());
    }

    auto future = QtConcurrent::mapped(found_skins, [](const QString& path) -> LoadedSkin {
        QList<core::Error> errors;
        auto result = core::read_package(path, errors);

        return { result, errors };
    });

    m_loading_watcher.setFuture(future);
}

void Engine::on_loading_completed()
{
    auto results = m_loading_watcher.future().results();

    std::size_t succeded_count;
    std::size_t failed_count;

    QList<core::Error> errors;
    for(auto result : results) {
        if(!result.result.has_value()) {
            failed_count++;
            errors.append(result.errors);
            continue;
        }

        auto value = result.result.value();

        m_skin_changer->add_skin(value);

        succeded_count++;
    }

    emit loading_finished(succeded_count, failed_count, errors);
}
