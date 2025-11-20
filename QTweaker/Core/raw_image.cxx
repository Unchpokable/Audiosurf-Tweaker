#include "precompiled.hxx"

#include "raw_image.hxx"

#include "image_processing.h"

std::optional<core::RawImage> core::RawImage::read(std::string_view path, CompressionLevel compression)
{
    if(!std::filesystem::exists(path)) {
        return std::nullopt;
    }

    auto name = std::filesystem::path(path).filename();

    auto format = image_format(path.data());

    if(format == NativeFormat::Unsupported) {
        return std::nullopt;
    }

    auto my_format = static_cast<ImageFormat>(format);

    std::int32_t width, height, channels;
    auto result = image_info(path.data(), &width, &height, &channels);

    if(result != Success) {
        return std::nullopt;
    }

    unsigned char* data;

    result = generic_decode(path.data(), &data, &width, &height, &channels);

    if(result != Success) {
        return std::nullopt;
    }

    auto compressed = qCompress(QByteArray(reinterpret_cast<char*>(data), width * height * channels), static_cast<int>(compression));

    free_image(data);
    return RawImage(compressed, my_format, QString::fromStdString(name.string()), width, height, channels);
}

std::optional<core::RawImage> core::RawImage::from_raw(const QByteArray& bytes, CompressionLevel compression)
{
    if(bytes.size() <= 0) {
        return std::nullopt;
    }

    auto format = image_format_from_mem(reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size());

    if(format == NativeFormat::Unsupported) {
        return std::nullopt;
    }

    auto my_format = static_cast<ImageFormat>(format);

    std::int32_t width, height, channels;

    auto result = image_info_from_mem(reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size(), &width, &height, &channels);

    if(result != Success) {
        return std::nullopt;
    }

    unsigned char* data;

    result = decode_from_mem(reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size(), &data, &width, &height, &channels);

    if(result != Success) {
        return std::nullopt;
    }

    auto compressed = qCompress(QByteArray(reinterpret_cast<char*>(data), width * height * channels), static_cast<int>(compression));

    free_image(data);

    return RawImage(compressed, my_format, "", width, height, channels);
}

core::RawImage::RawImage()
{
}

core::RawImage::RawImage(const RawImage& other)
    : m_data(other.m_data), m_format(other.m_format), m_name(other.m_name), m_width(other.m_width), m_height(other.m_height),
      m_channels_count(other.m_channels_count), m_meta_info(other.m_meta_info)
{
}

core::RawImage::RawImage(RawImage&& other)
    : m_data(std::move(other.m_data)), m_format(other.m_format), m_name(std::move(other.m_name)), m_width(other.m_width),
      m_height(other.m_height), m_channels_count(other.m_channels_count), m_meta_info(std::move(other.m_meta_info))
{
}

core::RawImage::~RawImage()
{
}

void core::RawImage::stream_insert(QDataStream& stream) const
{
    stream << static_cast<std::int32_t>(m_format);
    stream << m_width;
    stream << m_height;
    stream << m_channels_count;

    stream << m_name;

    stream << m_data;
    stream << m_meta_info;
}

void core::RawImage::stream_exctract(QDataStream& stream)
{
    std::int32_t raw_format {};
    stream >> raw_format;
    m_format = static_cast<ImageFormat>(raw_format);

    stream >> m_width;
    stream >> m_height;
    stream >> m_channels_count;

    stream >> m_name;

    stream >> m_data;
    stream >> m_meta_info;
}

bool core::RawImage::write(std::string_view path, ImageFormat format)
{
    if(!std::filesystem::exists(path)) {
        std::error_code errc;
        if(!std::filesystem::create_directories(path, errc)) {
            return false;
        }
    }

    std::filesystem::path file(std::string(path) + m_name.toStdString());

    auto decompressed = qUncompress(m_data);

    Result result;

    switch(format) {
        case Jpeg:
            result = encode_jpeg(
                file.string().c_str(), reinterpret_cast<unsigned char*>(decompressed.data()), m_width, m_height, m_channels_count, 90);
            break;

        case Png:
            result = encode_png(
                file.string().c_str(), reinterpret_cast<unsigned char*>(decompressed.data()), m_width, m_height, m_channels_count);
            break;

        case Unsupported:
            return false;
    }

    return result == Success;
}

bool core::RawImage::write(std::string_view path)
{
    return write(path, m_format);
}

QByteArray& core::RawImage::data()
{
    return m_data;
}

int32_t core::RawImage::width() const
{
    return m_width;
}

int32_t core::RawImage::height() const
{
    return m_height;
}

int32_t core::RawImage::channels_count() const
{
    return m_channels_count;
}

void core::RawImage::set_name(const QString& name)
{
    m_name = name;
}

QStringView core::RawImage::name() const
{
    return m_name;
}

core::RawImage::RawImage(QByteArray data, ImageFormat format, QString name, int32_t width, int32_t height, int32_t channels_count)
    : m_data(std::move(data)), m_format(format), m_name(std::move(name)), m_width(width), m_height(height), m_channels_count(channels_count)
{
}
