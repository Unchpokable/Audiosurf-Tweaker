#include "precompiled.hxx"

#include "skin_changer.hxx"

#include "engine.hxx"

#include "installer.hxx"

SkinChangerBackend::SkinChangerBackend(Engine* engine) : QObject(engine), m_engine(engine)
{
    assert(m_engine);
}

SkinsViewModel* SkinChangerBackend::model()
{
    return m_skins_list_model;
}

void SkinChangerBackend::add_skin(SkinItem* item)
{
    m_skins_list_model->addSkin(item);
}

void SkinChangerBackend::remove_skin(const QString& name)
{
    m_skins_list_model->removeSkin(name);
}

void SkinChangerBackend::remove_skin(int index)
{
    m_skins_list_model->removeSkin(index);
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

void SkinChangerBackend::install_configured(int index)
{
    assert(m_skins_list_model);
    assert(m_engine);

    auto skin = m_skins_list_model->getSkinItem(index);
    auto path = m_engine->settings()->textures_root();

    core::InstallFilter filter;

    filter.install_cliffs = true;
    filter.install_hits = m_install_hits;
    filter.install_particles = m_install_particles;
    filter.install_rings = m_install_rings;
    filter.install_tiles = m_install_tiles;
    filter.install_skyspheres = m_install_skyspheres;

    core::install_skin(path, skin->data(), filter);
}

void SkinChangerBackend::install_full(int index)
{
    assert(m_skins_list_model);
    assert(m_engine);

    auto skin = m_skins_list_model->getSkinItem(index);
    auto path = m_engine->settings()->textures_root();

    core::install_skin(path, skin->data());
}

void SkinChangerBackend::add_skin(core::PackData& skin_data)
{
    auto skin = new SkinItem(skin_data);
    m_skins_list_model->addSkin(skin);
}
