#include "precompiled.hxx"

#include "qmetacompat.hxx"

#include "Backend/skin_item.hxx"

#include "Backend/color_configurator.hxx"
#include "Backend/skin_changer.hxx"
#include "Backend/tweaker.hxx"
#include "Backend/engine.hxx"

QDataStream& operator<<(QDataStream& stream, const QList<QImage>& list)
{
    stream << list.size();
    for (const auto& image : list) {
        stream << image;
    }
    return stream;
}

QDataStream& operator>>(QDataStream& stream, QList<QImage>& list)
{
    int size;
    stream >> size;
    list.clear();
    list.reserve(size);
    for (int i = 0; i < size; ++i) {
        QImage image;
        stream >> image;
        list.append(image);
    }
    return stream;
}

void qt::dirty::compat_init()
{
    qRegisterMetaType<SkinItem>();
    qRegisterMetaType<QImageList>();

    qRegisterMetaType<ColorConfiguratorBackend>();
    qRegisterMetaType<SkinChangerBackend>();
    qRegisterMetaType<TweakerBackend>();
    qRegisterMetaType<Engine>();

    qRegisterMetaType<SkinItem*>();
    qRegisterMetaType<QImageList*>();

    qRegisterMetaType<ColorConfiguratorBackend*>();
    qRegisterMetaType<SkinChangerBackend*>();
    qRegisterMetaType<TweakerBackend*>();
    qRegisterMetaType<Engine*>();
}
