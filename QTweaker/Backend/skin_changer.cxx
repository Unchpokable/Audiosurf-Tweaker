#include "precompiled.hxx"

#include "skin_changer.hxx"

SkinChangerBackend::SkinChangerBackend(QObject* parent) : QObject(parent)
{
}

SkinsViewModel* SkinChangerBackend::model()
{
    return m_skins_list_model;
}

void SkinChangerBackend::add_skin(const SkinItem* item)
{
}

void SkinChangerBackend::remove_skin(const QString& name)
{
}

void SkinChangerBackend::remove_skin(int index)
{
}

void SkinChangerBackend::config_install_tiles(bool value)
{
    if(value != m_install_tiles) {
        m_install_tiles = value;
        emit configuration_changed();
    }
}

void SkinChangerBackend::config_install_rings(bool value)
{
    if(value != m_install_rings) {
        m_install_rings = value;
        emit configuration_changed();
    }
}

void SkinChangerBackend::config_install_hits(bool value)
{
    if(value != m_install_hits) {
        m_install_hits = value;
        emit configuration_changed();
    }
}

void SkinChangerBackend::config_install_particles(bool value)
{
    if(value != m_install_particles) {
        m_install_particles = value;
        emit configuration_changed();
    }
}

void SkinChangerBackend::config_install_skyspheres(bool value)
{
    if(value != m_install_skyspheres) {
        m_install_skyspheres = value;
        emit configuration_changed();
    }
}

void SkinChangerBackend::install_configured()
{
}

void SkinChangerBackend::install_full()
{
}
