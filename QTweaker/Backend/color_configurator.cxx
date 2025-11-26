#include "precompiled.hxx"

#include "color_configurator.hxx"
#include "engine.hxx"

ColorConfiguratorBackend::ColorConfiguratorBackend(Engine* engine) : QObject(engine), m_engine(engine)
{
    assert(m_engine);
}
