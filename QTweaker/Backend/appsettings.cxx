#include "precompiled.hxx"

#include "appsettings.hxx"

QString AppSettingsBackend::textures_root() const
{
    return m_game_path;
}

void AppSettingsBackend::set_textures_root(const QString& data)
{
    m_game_path = data;
}
