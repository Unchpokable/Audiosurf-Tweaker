#ifndef PACKAGER_H
#define PACKAGER_H

#include "error.hxx"

#include "Core_global.h"

namespace core
{
static constexpr char file_extension[] = "tpack";
} // namespace core

namespace core
{
struct PackData;
} // namespace core

namespace core
{
CORE_EXPORT std::optional<core::PackData> read_package(std::string_view path, QList<Error>& errors);
} // namespace core

namespace core
{
CORE_EXPORT bool write_package(const core::PackData& data, std::string_view output_path);
} // namespace core

#endif
