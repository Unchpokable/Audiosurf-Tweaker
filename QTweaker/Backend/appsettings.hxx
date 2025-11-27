#pragma once

#include "Backend_global.h"

class Engine;

class BACKEND_EXPORT AppSettingsBackend : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString textures_root READ textures_root WRITE set_textures_root NOTIFY textures_root_changed)
    Q_PROPERTY(bool use_hot_reload READ use_hot_reload WRITE set_use_hot_reload NOTIFY use_hot_reload_changed)
    Q_PROPERTY(bool use_safe_install READ use_safe_install WRITE set_use_safe_install NOTIFY use_safe_install_changed)
    Q_PROPERTY(bool use_texture_tracking READ use_texture_tracking WRITE set_use_texture_tracking NOTIFY use_texture_tracking_changed)

public:
    explicit AppSettingsBackend(Engine* engine);

    QString textures_root() const;
    void set_textures_root(const QString& data);

    bool use_hot_reload() const;
    void set_use_hot_reload(bool use);

    bool use_safe_install() const;
    void set_use_safe_install(bool use);

    bool use_texture_tracking() const;
    void set_use_texture_tracking(bool use);

signals:
    void textures_root_changed() const;
    void use_hot_reload_changed() const;
    void use_safe_install_changed() const;
    void use_texture_tracking_changed() const;

private:
    QString m_game_path;

    bool m_use_hot_reload;
    bool m_use_safe_install;
    bool m_use_textures_tracking;

    Engine* m_engine;
};

Q_DECLARE_METATYPE(AppSettingsBackend);
Q_DECLARE_METATYPE(AppSettingsBackend*);
