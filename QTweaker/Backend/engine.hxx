#ifndef ENGINE_H
#define ENGINE_H

#include "Backend_global.h"

#include "appsettings.hxx"
#include "color_configurator.hxx"
#include "skin_changer.hxx"
#include "tweaker.hxx"

class BACKEND_EXPORT Engine : public QObject {
    Q_OBJECT
    Q_PROPERTY(SkinChangerBackend* skin_changer READ skin_changer CONSTANT)
    Q_PROPERTY(TweakerBackend* tweaker READ tweaker CONSTANT)
    Q_PROPERTY(ColorConfiguratorBackend* color_configurator READ color_configurator CONSTANT)
    Q_PROPERTY(AppSettingsBackend* settings READ settings CONSTANT)

    struct LoadedSkin {
        std::optional<core::PackData> result;
        QList<core::Error> errors;
    };

    using LoadingWatcher = QFutureWatcher<LoadedSkin>;

public:
    Q_INVOKABLE explicit Engine(QObject* parent = nullptr);

    Q_INVOKABLE SkinChangerBackend* skin_changer();
    Q_INVOKABLE TweakerBackend* tweaker();
    Q_INVOKABLE ColorConfiguratorBackend* color_configurator();
    Q_INVOKABLE AppSettingsBackend* settings();

    Q_INVOKABLE void startup();

signals:
    /// Emits when Tweaker loads successfully
    void startup_success();

    /// Emits when Tweaker unable to load - missing required resources, wrong configuration, etc.
    void startup_failed(QString reason);

    // Daisy-chainged signals for loading

    void loading_range_updated(int min, int max);
    void loading_value_changed(int val);
    void loading_finished(std::size_t loaded_count, std::size_t failed_count, QList<core::Error> errors);

private slots:
    void on_loading_completed();

private:
    SkinChangerBackend* m_skin_changer;
    TweakerBackend* m_tweaker;
    ColorConfiguratorBackend* m_color_configurator;
    AppSettingsBackend* m_settings;

    LoadingWatcher m_loading_watcher;
};

Q_DECLARE_METATYPE(Engine);
Q_DECLARE_METATYPE(Engine*);

#endif // ENGINE_H
