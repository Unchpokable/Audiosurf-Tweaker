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
