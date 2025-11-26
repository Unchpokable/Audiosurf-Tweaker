#include "precompiled.hxx"

#include "backgrounds_provider.hxx"
#include "engine.hxx"
#include "image_utils.hxx"
#include "skin_changer.hxx"
#include "skins_view_model.hxx"

BackgroundsProvider::BackgroundsProvider(Engine* engine) : QQuickImageProvider(QQuickImageProvider::Image), m_engine(engine)
{
}

QImage BackgroundsProvider::requestImage(const QString& id, QSize* size, const QSize& requestedSize)
{
    if(m_cached.contains(id)) {
        return m_cached.value(id);
    }

    auto item = m_engine->skin_changer()->model()->getSkinItem(id);
    assert(item); // should not be null in a normal working scenario

    auto previews = item->previews();

    if(previews.isEmpty()) {
        return QImage();
    }

    auto image = previews.first();

    auto blurred = utils::image::blur(image, 3);

    m_cached.insertOrAssign(id, blurred);

    if(requestedSize.isValid()) {
        blurred = blurred.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    return blurred;
}

void BackgroundsProvider::invalidateObjectBackgroundCache(const QString& id)
{
    if(m_cached.contains(id)) {
        m_cached.remove(id);
    }
}
