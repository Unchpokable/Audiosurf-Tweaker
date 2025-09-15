#ifndef PACK_DATA_H
#define PACK_DATA_H

#include "raw_image.hxx"

namespace core
{
struct PackData
{
    std::string name;

    std::vector<RawImage> required_parts;
    std::vector<RawImage> optional_parts;
    std::vector<RawImage> previews;
};
}

#endif // PACK_DATA_H
