#include "precompiled.hxx"

#include "skin_item.hxx"

SkinItem::SkinItem(core::PackData& data, QObject* parent) : QObject(parent)
{
    m_previews_cache.clear();
    m_data = data;
}

void SkinItem::rename(const QString& name)
{
    m_data.name = name;
    emit renamed(name);
}

void SkinItem::set_author(const QString& author)
{
    m_data.author = author;
    emit author_changed(author);
}

const QImageList& SkinItem::previews()
{
    if(m_previews_cache.empty()) {
        for(auto& image : m_data.previews) {
            auto unpacked = qUncompress(image.data());
            QImage qimage(reinterpret_cast<uchar*>(unpacked.data()), image.width(), image.height(), deduce_format(image));

            m_previews_cache.append(qimage.copy());
        }
    }

    return m_previews_cache;
}

const core::PackData& SkinItem::data() const
{
    return m_data;
}

QImage::Format SkinItem::deduce_format(const core::RawImage& image) const
{
    QImage::Format format;
    switch(image.channels_count()) {
        case 1:
            format = QImage::Format_Grayscale8;
            break;
        case 3:
            format = QImage::Format_RGB888;
            break;
        case 4:
            format = QImage::Format_RGBA8888;
            break;
        default:
            format = QImage::Format_Invalid;
    }

    return format;
}
