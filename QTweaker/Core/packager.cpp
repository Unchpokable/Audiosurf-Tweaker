#include "precompiled.hxx"

#include "packager.hxx"

#include "pack_data.hxx"

#include "raw_image.hxx"

namespace
{
static constexpr char required_header[] = "TWEAKER_SKIN0000";
static constexpr std::uint16_t serializer_version_major = 1;
static constexpr std::uint16_t serializer_version_minor = 0;
static constexpr std::uint16_t serializer_version_patch = 0;
static constexpr char file_extension[] = "tpack";
}

std::optional<core::PackData> core::read_package(std::string_view path, QList<Error>& errors)
{
    if(!std::filesystem::exists(path)) {
        return std::nullopt;
    }

    PackData data;

    QFile file(path.data());

    file.open(QIODevice::OpenModeFlag::ReadOnly);

    if(file.size() < sizeof(required_header) + sizeof(std::uint16_t) * 3) {
        return std::nullopt;
    }

    QDataStream stream(&file);

    char header[sizeof(required_header)];

    stream.readRawData(header, sizeof(header));

    if(std::strcmp(header, required_header) != 0) {
        errors << make_error(core::Severe, "Incompatible file!", "Unable to recognize header. File incompatible or corrupted");
        return std::nullopt;
    }

    std::uint16_t maj_ver, min_ver, patch_ver;

    stream >> maj_ver;
    stream >> min_ver;
    stream >> patch_ver;

    if(maj_ver != serializer_version_major) {
        errors << make_error(core::Severe, "Incompatible file!", "Major serializer versions are different. File format not supported by current serializer version");
        return std::nullopt;
    }

    if(min_ver != serializer_version_minor) {
        errors << make_error(core::Minor, "Potantially incompatible!", "Different minor version. File may be saved in older or newer program version!");
    }

    if(patch_ver != serializer_version_patch) {
        errors << make_error(core::Minor, "Potantially incompatible!", "Different patch version. File may be saved in older or newer program version!");
    }

    QByteArray package_name;

    stream >> data.name;

    std::size_t required_parts_count {};

    stream.readRawData(reinterpret_cast<char*>(&required_parts_count), sizeof(std::size_t));

    QByteArray required_block;

    stream >> required_block;
}

bool core::write_package(const PackData &data, std::string_view output_path)
{
    QByteArray content;

    QDataStream stream(&content, QIODevice::OpenModeFlag::WriteOnly);

    stream.writeRawData(required_header, sizeof(required_header));

    stream << serializer_version_major;
    stream << serializer_version_minor;
    stream << serializer_version_patch;

    stream << data.name;

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
        stream << 1; // Optional parts presented

        stream.writeRawData(reinterpret_cast<char*>(&optional_parts_count), sizeof(std::size_t));

        QByteArray optional_data_block;
        QDataStream optional_data_stream(&optional_data_block, QIODevice::OpenModeFlag::WriteOnly);

        for(auto& image : data.optional_parts) {
            image.stream_insert(optional_data_stream);
        }

        stream << optional_data_block;
    }
    else {
        stream << 0; // No optional parts presented
    }

    std::size_t previews_count = data.previews.size();

    if(previews_count > 0) {
        stream << 1; // Previews presented

        stream.writeRawData(reinterpret_cast<char*>(&previews_count), sizeof(std::size_t));

        QByteArray previews_data_block;
        QDataStream previews_data_stream(&previews_data_block, QIODevice::OpenModeFlag::WriteOnly);

        for(auto& image : data.previews) {
            image.stream_insert(previews_data_stream);
        }

        stream << previews_data_block;
    }
    else {
        stream << 0; // no previews presented;
    }

    if(stream.status() == QDataStream::Ok) {
        auto file = std::filesystem::path(data.name.toStdString());
        file.replace_extension(file_extension);

        auto full_path = std::filesystem::path(output_path) / file;

        QFile output(full_path.c_str());

        output.open(QIODevice::OpenModeFlag::WriteOnly | QIODevice::OpenModeFlag::Truncate);

        QDataStream output_stream(&output);

        output_stream << content;

        output.close();

        return true;
    }

    return false;
}
