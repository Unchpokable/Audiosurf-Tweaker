#ifndef ZIP_SUPPORT_HXX
#define ZIP_SUPPORT_HXX

#include "Core_global.h"

#include "error.hxx"
#include "pack_data.hxx"

namespace core::zip
{
struct ZipEntry {
    QString name;
    QString root;

    std::size_t compressed_size;
    std::size_t total_size;

    std::int16_t compression_method;

    bool is_directory;
};
} // namespace core::zip

namespace core::zip
{
CORE_EXPORT Error read_archive_raw(QStringView path, QList<core::zip::ZipEntry>& result);
CORE_EXPORT Error read_archive(QStringView path, core::PackData& result);
} // namespace core::zip

namespace core::zip
{
CORE_EXPORT QString write_archive(const core::PackData& data, QStringView path_to_save);
CORE_EXPORT QString write_archive_raw(const QList<core::zip::ZipEntry>& entries, QStringView path_to_save);
} // namespace core::zip

#endif // ZIP_SUPPORT_HXX
