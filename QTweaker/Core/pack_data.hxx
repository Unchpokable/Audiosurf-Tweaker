#ifndef PACK_DATA_H
#define PACK_DATA_H

#include "raw_image.hxx"

namespace core
{

struct PackData
{
    QString name;
    QString author;

    QList<RawImage> required_parts;
    QList<RawImage> optional_parts;
    QList<RawImage> previews;
};

}

#endif
