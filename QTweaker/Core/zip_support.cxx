#include "precompiled.hxx"

#include "zip_handle.hxx"
#include "zip_support.hxx"
#include "logging.hxx"

#include "zip.h"

namespace
{
std::filesystem::path full_path(core::zip::ZipEntry& entry)
{
    auto root = std::filesystem::path(entry.root.toStdString());
    auto file = std::filesystem::path(entry.name.toStdString());

    return root / file;
}

bool quick_compare_pattern(std::string_view source, std::vector<const char*> patterns)
{
    for(std::string_view pattern : patterns) {

    }
}
}

core::Error core::zip::read_archive_raw(QStringView path, QList<core::zip::ZipEntry>& result)
{
    auto zip = ZipHandle(path, ZipHandle::Discard);

    if(!core::is_fine(zip.error())) {
        return zip.error();
    }

    zip_int64_t entries_count = zip_get_num_entries(zip, 0);

    for(zip_int64_t i { 0 }; i < entries_count; ++i) {
        zip_stat_t file_stat;

        if(zip_stat_index(zip, i, 0, &file_stat) == 0) {
            ZipEntry entry;

            std::filesystem::path path(file_stat.name);

            entry.name = QString::fromStdString(path.filename().string());
            entry.root = QString::fromStdString(path.parent_path().string());

            entry.is_directory = std::string_view(file_stat.name).ends_with("/");

            entry.total_size = file_stat.size;
            entry.compressed_size = file_stat.comp_size;

            entry.compression_method = file_stat.comp_method;

            result.append(entry);
        }
    }

    return core::Error{};
}

core::Error core::zip::read_archive(QStringView path, core::PackData& result)
{
    QList<core::zip::ZipEntry> files;
    auto error = read_archive_raw(path, files);

    if(!core::is_fine(error)) {
        return error;
    }

    auto zip = ZipHandle(path, ZipHandle::Discard);

    if(!core::is_fine(zip.error())) {
        return zip.error();
    }

    core::PackData data;

    for(auto& entry : files) {
        auto path = full_path(entry);

        auto index = zip_name_locate(zip, path.string().c_str(), 0);
        if(index < 0) {
            LOG_WARNING("Unable to locate entry: {}", path.string());
            continue;
        }

        auto file = zip_fopen_index(zip, index, 0);
        if(!file) {
            LOG_WARNING("Unable to open file {} at index {}", path.string(), index);
        }

        std::vector<std::uint8_t> output;
        output.resize(entry.total_size);

        auto bytes_read = zip_fread(file, output.data(), entry.total_size);
        if(bytes_read != entry.total_size) {
            LOG_WARNING("Unable to read file. Declared size: {}, bytes read: {}", entry.total_size, bytes_read);
        }



        auto image = RawImage::from_raw(QByteArray(reinterpret_cast<const char*>(output.data()), output.size()));
    }
}

QString core::zip::write_archive(const PackData& data, QStringView path_to_save)
{
    return {};
}

QString core::zip::write_archive_raw(const QList<ZipEntry>& entries, QStringView path_to_save)
{
    return {};
}
