#include "precompiled.hxx"

#include "packager.hxx"

#include "pack_data.hxx"

#include "raw_image.hxx"

namespace
{
static constexpr char required_header[] = "TWEAKER_SKIN0000";
static constexpr std::uint16_t version[3] = { 1, 0, 0 };
}

std::optional<core::PackData> core::read_package(std::string_view path)
{
    if(!std::filesystem::exists(path)) {
        return std::nullopt;
    }

    PackData data;
}

bool core::write_package(const PackData &data, std::string_view output_path)
{
    QByteArray block;

    QDataStream stream(&block, QIODevice::OpenModeFlag::WriteOnly);

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

    return stream.status() == QDataStream::Ok;
}
