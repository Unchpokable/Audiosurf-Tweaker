#include "precompiled.hxx"

#include "engine.hxx"

Engine::Engine(QObject* parent) : QObject(parent)
{
    m_skin_changer = new SkinChangerBackend(this);
    m_tweaker = new TweakerBackend(this);
    m_color_configurator = new ColorConfiguratorBackend(this);
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
