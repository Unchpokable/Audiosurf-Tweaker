#include "skin_item.hxx"

SkinItem::SkinItem(
    QObject* parent)
    : QObject(parent)
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
    QList<QImage> result;

    for(auto& image: m_data.previews) {
        auto unpacked = qUncompress(image.data());
        QImage qimage(reinterpret_cast<uchar*>(unpacked.data()), image.width(), image.height(), deduce_format(image));

        result.append(qimage.copy());
    }

    return result;
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
