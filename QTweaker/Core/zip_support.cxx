#include "precompiled.hxx"

#include "zip_handle.hxx"
#include "zip_support.hxx"
#include "logging.hxx"
#include "image_processing.h"

#include "masks.hxx"

#include "zip.h"

namespace
{
std::filesystem::path full_path(core::zip::ZipEntry& entry)
{
    auto root = std::filesystem::path(entry.root.toStdString());
    auto file = std::filesystem::path(entry.name.toStdString());

    return root / file;
}

bool ends_with_any(std::string_view source, std::vector<const char*> patterns)
{
    for(std::string_view pattern : patterns) {
        if(source.ends_with(pattern)) {
            return true;
        }
    }

    return false;
}
}

core::Error core::zip::read_archive_raw(QStringView path, QList<core::zip::ZipEntry>& result)
{
    ZipHandle zip(path, ZIP_RDONLY, ZipHandle::DefaultFinalize::Discard);

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

    ZipHandle zip(path, ZIP_RDONLY, ZipHandle::DefaultFinalize::Discard);

    if(!core::is_fine(zip.error())) {
        return zip.error();
    }

    core::Rank error_severity { core::Nothing };

    for(auto& entry : files) {
        if(entry.is_directory) {
            continue;
        }

        auto path = full_path(entry);

        auto index = zip_name_locate(zip, path.string().c_str(), 0);
        if(index < 0) {
            LOG_WARNING("Unable to locate entry: {}", path.string());
            error_severity = core::Moderate;
            continue;
        }

        auto file = zip_fopen_index(zip, index, 0);
        if(!file) {
            LOG_WARNING("Unable to open file {} at index {}", path.string(), index);
            error_severity = core::Moderate;
            continue;
        }

        std::vector<std::uint8_t> output;
        output.resize(entry.total_size);

        auto bytes_read = zip_fread(file, output.data(), entry.total_size);
        if(bytes_read != entry.total_size) {
            LOG_WARNING("Unable to read file. Declared size: {}, bytes read: {}", entry.total_size, bytes_read);
            error_severity = core::Moderate;
            zip_fclose(file);
            continue;
        }
        zip_fclose(file);

        auto ftype = image_format_from_mem(output.data(), output.size());
        if(ftype == Unsupported) {
            LOG_INFO("Archive contains a non-image data, skipping: {}", entry.name.toStdString());
            continue;
        }

        auto image = RawImage::from_raw(QByteArray(reinterpret_cast<const char*>(output.data()), output.size()));

        if(!image) {
            LOG_WARNING("Unable to read Image: {}", entry.name.toStdString());
            error_severity = core::Moderate;

            continue;
        }

        image->set_name(entry.name);

        if(ends_with_any(path.string(), core::masks::required_files)) {
            result.required_parts.append(*image);
        }
        else if(ends_with_any(path.string(), core::masks::optional_files)) {
            result.optional_parts.append(*image);
        }
        else if(core::masks::preview_screenshots.match(QString::fromStdString(path.string())).hasMatch()) {
            result.previews.append(*image);
        }
        else {
            LOG_WARNING("Unknown file: {}, can not match with any known files", entry.name.toStdString());
        }
    }

    if(error_severity != core::Nothing) {
        return core::make_error(error_severity, "Archive reading error!", "Errors occured during reading an archive, check the results!");
    }

    return core::Error{};
}

QString core::zip::write_archive(const PackData& data, QStringView path_to_save)
{
    // todo: currently unsupported
    return {};
}

QString core::zip::write_archive_raw(const QList<ZipEntry>& entries, QStringView path_to_save)
{
    // todo: currently unsupported
    return {};
}
