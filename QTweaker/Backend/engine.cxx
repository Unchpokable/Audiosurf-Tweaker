#include "precompiled.hxx"

#include "engine.hxx"

#include "pack_data.hxx"
#include "packager.hxx"
#include "zip_support.hxx"

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

    QStringList filters;
    filters << QString::fromStdString(std::format(".{}", core::file_extension));
    filters << ".zip";

    QDirIterator dir_iterator(local_skins_folder, filters, QDir::Files);

    while(dir_iterator.hasNext()) {
        auto next = dir_iterator.next();
        found_skins.append(QFileInfo(next).fileName());
    }

    auto future = QtConcurrent::mapped(found_skins, [](const QString& path) -> LoadedSkin {
        QList<core::Error> errors;

        auto ext = QFileInfo(path).suffix();

        std::optional<core::PackData> result { std::nullopt };

        if(ext.contains(core::file_extension)) {
            result = core::read_package(path, errors);
        }
        else if(ext.contains("zip")) {
            core::PackData tmp;
            auto err = core::zip::read_archive(path, tmp);

            if(err.error_rank == core::Nothing) {
                result = tmp;
            }
            else {
                errors.append(err);
            }
        }
        else {
            errors.append(core::make_error(
                core::Rank::Minor, "Unsaupported file!", "File {} unable to read because it is unsupported", path.toStdString()));
        }

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
        if(!result.data.has_value()) {
            failed_count++;
            errors.append(result.errors);
            continue;
        }

        auto value = result.data.value();

        m_skin_changer->add_skin(value);

        succeded_count++;
    }

    emit loading_finished(succeded_count, failed_count, errors);
}
