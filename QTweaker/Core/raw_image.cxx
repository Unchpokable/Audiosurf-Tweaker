#include "precompiled.hxx"

#include "raw_image.hxx"

#include "image_processing.h"


std::optional<core::RawImage> core::RawImage::from_file(std::string_view path, CompressionLevel compression)
{
    if(!std::filesystem::exists(path)) {
        return std::nullopt;
    }

    auto format = get_image_format_native(path.data());

    if(format == NativeFormat::Unsupported) {
        return std::nullopt;
    }

    auto my_format = static_cast<ImageFormat>(format);

    std::int32_t width, height, channels;
    auto result = image_info(path.data(), &width, &height, &channels);

    if(result != Success) {
        return std::nullopt;
    }

    auto data = new unsigned char[width * height * channels];

    result = generic_decode(path.data(), &data, &width, &height, &channels);

    if(result != Success) {
        delete[] data;
        return std::nullopt;
    }

    auto compressed = qCompress(QByteArray(reinterpret_cast<char*>(data), width * height * channels), static_cast<int>(compression));

    delete[] data;
    return RawImage(compressed, my_format, width, height, channels);
}

void core::RawImage::stream_insert(const QDataStream &stream)
{

}

void core::RawImage::stream_exctract(const QDataStream &stream)
{

}

core::RawImage::RawImage(QByteArray data, ImageFormat format, int32_t width, int32_t height, int32_t channels_count)
    : m_data(std::move(data)), m_format(format), m_width(width), m_height(height), m_channels_count(channels_count) {  }
