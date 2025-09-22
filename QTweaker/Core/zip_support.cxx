#include "precompiled.hxx"

#include "zip_support.hxx"

#include "zip.h"

core::Error core::zip::read_archive_raw(QStringView path, QList<core::zip::ZipEntry>& result)
{
    int errp;
    auto zip = zip_open(path.toUtf8().constData(), ZIP_RDONLY, &errp);

    if(zip == nullptr) {
        zip_error_t libzip_error;
        zip_error_init_with_code(&libzip_error, errp);

        auto error = core::make_error(core::Minor, "Archive reading error!", "Unable to open archive {}, libzip failed: {}",
            path.toString().toStdString(), zip_error_strerror(&libzip_error));

        zip_error_fini(&libzip_error);

        return error;
    }

    zip_int64_t entries_count = zip_get_num_entries(zip, 0);

    for(zip_int64_t i { 0 }; i < entries_count; ++i) {
        zip_stat_t file_stat;

        if(zip_stat_index(zip, i, 0, &file_stat) == 0) {
            core::zip::ZipEntry entry;

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

    zip_close(zip);

    return core::Error{};
}

core::Error core::zip::read_archive(QStringView path, core::PackData& result)
{
    QList<core::zip::ZipEntry> files;
    auto error = read_archive_raw(path, files);

    if(!core::is_fine(error)) {
        return error;
    }

}

QString core::zip::write_archive(const PackData& data, QStringView path_to_save)
{
}

QString core::zip::write_archive_raw(const QList<ZipEntry>& entries, QStringView path_to_save)
{
}
