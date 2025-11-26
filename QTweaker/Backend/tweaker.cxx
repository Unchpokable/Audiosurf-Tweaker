#include "precompiled.hxx"

#include "engine.hxx"
#include "tweaker.hxx"

TweakerBackend::TweakerBackend(Engine* engine) : QObject(engine), m_engine(engine)
{
    assert(m_engine);
}
