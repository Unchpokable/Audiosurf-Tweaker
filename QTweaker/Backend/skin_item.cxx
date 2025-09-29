#include "skin_item.hxx"

SkinItem::SkinItem(QObject* parent) : QObject(parent)
{
}

void SkinItem::rename(
    const QString& name)
{
}

void SkinItem::set_author(const QString &author)
{
}

QImageList SkinItem::previews()
{
    if(m_previews_cache.empty()) {
        for(auto& image: m_data.previews) {
            auto unpacked = qUncompress(image.data());
            QImage qimage(reinterpret_cast<uchar*>(unpacked.data()), image.width(), image.height(), deduce_format(image));

            m_previews_cache.append(qimage.copy());
        }
    }

    return m_previews_cache;
}

QImage::Format SkinItem::deduce_format(const core::RawImage &image) const
{
    QImage::Format format;
    switch(image.channels_count()) {
        case 1: format = QImage::Format_Grayscale8; break;
        case 3: format = QImage::Format_RGB888; break;
        case 4: format = QImage::Format_RGBA8888; break;
        default:
            format = QImage::Format_Invalid;
    }

    return format;
}
