#include "precompiled.hxx"

#include "packager.hxx"

#include "logging.hxx"
#include "pack_data.hxx"
#include "raw_image.hxx"

namespace
{
static constexpr char required_header[] = "TWEAKER_SKIN0000";
static constexpr std::uint16_t serializer_version_major = 1;
static constexpr std::uint16_t serializer_version_minor = 0;
static constexpr std::uint16_t serializer_version_patch = 0;
} // namespace

std::optional<core::PackData> core::read_package(const QString& path, QList<Error>& errors)
{
    if(!QFile::exists(path)) {
        return std::nullopt;
    }

    PackData data;

    QFile file(path);

    if(!file.open(QIODevice::OpenModeFlag::ReadOnly)) {
        LOG_WARNING("Failed to open {}", path.toStdString());
        return std::nullopt;
    }

    if(file.size() < sizeof(required_header) + sizeof(std::uint16_t) * 3) {
        return std::nullopt;
    }

    QDataStream stream(&file);

    char header[sizeof(required_header)];

    stream.readRawData(header, sizeof(header));

    if(std::memcmp(header, required_header, sizeof(required_header) - 1) != 0) {
        errors << make_error(core::Severe, "Incompatible file!", "Unable to recognize header. File incompatible or corrupted");
        return std::nullopt;
    }

    std::uint16_t maj_ver, min_ver, patch_ver;

    stream >> maj_ver;
    stream >> min_ver;
    stream >> patch_ver;

    if(maj_ver != serializer_version_major) {
        errors << make_error(core::Moderate,
            "Incompatible file!",
            "Major serializer versions are different. File format not supported by current serializer version");
        return std::nullopt;
    }

    if(min_ver != serializer_version_minor) {
        errors << make_error(
            core::Minor, "Potantially incompatible!", "Different minor version. File may be saved in older or newer program version!");
    }

    if(patch_ver != serializer_version_patch) {
        errors << make_error(
            core::Minor, "Potantially incompatible!", "Different patch version. File may be saved in older or newer program version!");
    }

    stream >> data.name;

    stream >> data.author;

    std::size_t required_parts_count {};

    stream.readRawData(reinterpret_cast<char*>(&required_parts_count), sizeof(std::size_t));

    QByteArray required_block;

    stream >> required_block;

    QDataStream required_block_stream(&required_block, QIODevice::OpenModeFlag::ReadOnly);

    for(auto i { 0 }; i < required_parts_count; ++i) {
        RawImage image;
        image.stream_exctract(required_block_stream);

        data.required_parts.push_back(image);
    }

    std::int32_t has_optional_parts;
    stream >> has_optional_parts;

    if(has_optional_parts) {
        std::size_t optional_parts_count;
        stream.readRawData(reinterpret_cast<char*>(&optional_parts_count), sizeof(std::size_t));

        QByteArray optional_block;
        stream >> optional_block;

        QDataStream optional_stream(&optional_block, QIODevice::OpenModeFlag::ReadOnly);

        for(auto i { 0 }; i < optional_parts_count; ++i) {
            RawImage image;
            image.stream_exctract(optional_stream);

            data.optional_parts.push_back(image);
        }
    }

    std::int32_t has_previews;
    stream >> has_previews;

    if(has_previews) {
        std::size_t previews_count;
        stream.readRawData(reinterpret_cast<char*>(&previews_count), sizeof(std::size_t));

        QByteArray previews_block;
        stream >> previews_block;

        QDataStream previews_stream(&previews_block, QIODevice::OpenModeFlag::ReadOnly);

        for(auto i { 0 }; i < previews_count; ++i) {
            RawImage image;
            image.stream_exctract(previews_stream);

            data.previews.push_back(image);
        }
    }

    if(stream.status() != QDataStream::Status::Ok) {
        errors << make_error(core::Moderate, "Corrupted file", "Serializer can not load file properly!");
        return std::nullopt;
    }

    return data;
}

bool core::write_package(const PackData& data, const QString& output_path)
{
    QByteArray content;

    QDataStream stream(&content, QIODevice::OpenModeFlag::WriteOnly);

    stream.writeRawData(required_header, sizeof(required_header));

    stream << serializer_version_major;
    stream << serializer_version_minor;
    stream << serializer_version_patch;

    stream << data.name;

    stream << data.author;

    std::size_t required_parts_count = data.required_parts.size();

    if(required_parts_count == 0) {
        return false;
    }

    stream.writeRawData(reinterpret_cast<char*>(&required_parts_count), sizeof(std::size_t));

    QByteArray required_block;
    QDataStream required_data_stream(&required_block, QIODevice::OpenModeFlag::WriteOnly);

    for(auto& image : data.required_parts) {
        image.stream_insert(required_data_stream);
    }

    stream << required_block;

    std::size_t optional_parts_count = data.optional_parts.size();

    if(optional_parts_count > 0) {
        stream << static_cast<std::int32_t>(1);
        stream.writeRawData(reinterpret_cast<char*>(&optional_parts_count), sizeof(std::size_t));

        QByteArray optional_data_block;
        QDataStream optional_data_stream(&optional_data_block, QIODevice::OpenModeFlag::WriteOnly);

        for(auto& image : data.optional_parts) {
            image.stream_insert(optional_data_stream);
        }

        stream << optional_data_block;
    }
    else {
        stream << static_cast<std::int32_t>(0);
    }

    std::size_t previews_count = data.previews.size();

    if(previews_count > 0) {
        stream << static_cast<std::int32_t>(1);

        stream.writeRawData(reinterpret_cast<char*>(&previews_count), sizeof(std::size_t));

        QByteArray previews_data_block;
        QDataStream previews_data_stream(&previews_data_block, QIODevice::OpenModeFlag::WriteOnly);

        for(auto& image : data.previews) {
            image.stream_insert(previews_data_stream);
        }

        stream << previews_data_block;
    }
    else {
        stream << static_cast<std::int32_t>(0);
    }

    if(stream.status() == QDataStream::Ok) {
        QFileInfo file_info(data.name);
        QString file_name = file_info.completeBaseName() + "." + file_extension;

        QDir output_dir(output_path);
        QString full_path = output_dir.filePath(file_name);

        QFile output(full_path);

        if(!output.open(QIODevice::OpenModeFlag::WriteOnly | QIODevice::OpenModeFlag::Truncate)) {
            LOG_WARNING("Failed to open {}", output.fileName().toStdString());
            return false;
        }

        QDataStream output_stream(&output);

        output_stream << content;

        output.close();

        return true;
    }

    return false;
}
