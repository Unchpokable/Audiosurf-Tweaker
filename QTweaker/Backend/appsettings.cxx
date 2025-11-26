#include "precompiled.hxx"

#include "appsettings.hxx"

#include "engine.hxx"

AppSettingsBackend::AppSettingsBackend(Engine* engine) : m_engine(engine)
{
}

QString AppSettingsBackend::textures_root() const
{
    return m_game_path;
}

void AppSettingsBackend::set_textures_root(const QString& data)
{
    m_game_path = data;
}

bool AppSettingsBackend::use_hot_reload() const
{
    return m_use_hot_reload;
}

void AppSettingsBackend::set_use_hot_reload(bool use)
{
    m_use_hot_reload = use;
}

bool AppSettingsBackend::use_safe_install() const
{
    return m_use_safe_install;
}

void AppSettingsBackend::set_use_safe_install(bool use)
{
    m_use_safe_install = use;
}

bool AppSettingsBackend::use_texture_tracking() const
{
    return m_use_textures_tracking;
}

void AppSettingsBackend::set_use_texture_tracking(bool use)
{
    m_use_textures_tracking = use;
}
