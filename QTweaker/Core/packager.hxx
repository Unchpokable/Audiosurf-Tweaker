#ifndef PACKAGER_H
#define PACKAGER_H

#include "error.h"

namespace core
{
struct PackData;
}

namespace core
{
std::optional<core::PackData> read_package(std::string_view path, QList<Error>& errors);
}

namespace core
{
bool write_package(const core::PackData& data, std::string_view output_path);
}

#endif // PACKAGER_H
