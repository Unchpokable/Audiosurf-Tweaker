#include "precompiled.hxx"

#include "backgrounds_provider.hxx"

BackgroundsProvider::BackgroundsProvider(Engine* engine) : QQuickImageProvider(QQuickImageProvider::Image), m_engine(engine)
{
}
