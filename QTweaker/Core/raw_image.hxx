#ifndef RAW_IMAGE_HXX
#define RAW_IMAGE_HXX

#include "Core_global.h"

namespace core
{
class CORE_EXPORT RawImage
{
public:
    enum CompressionLevel : std::int32_t {
        Raw = 0,
        ZLib_Default = 6,
        Highest = 9
    };

    enum ImageFormat : std::int32_t
    {
        Jpeg = 0,
        Png,
        Unsupported
    };

public:
    explicit RawImage();
    RawImage(const RawImage& other);
    RawImage(RawImage&& other);

    static std::optional<RawImage> read(std::string_view path, CompressionLevel compression = ZLib_Default);

    void stream_insert(QDataStream& stream) const;
    void stream_exctract(QDataStream& stream);

    bool write(std::string_view path, ImageFormat format);

private:
    RawImage(QByteArray data, ImageFormat format, QString name, std::int32_t width, std::int32_t height, std::int32_t channels_count);

private:
    ImageFormat m_format;
    std::int32_t m_width;
    std::int32_t m_height;
    std::int32_t m_channels_count;

    QString m_name;

    QByteArray m_data;
    QHash<QString, QString> m_meta_info;
};
}

#endif // RAW_IMAGE_HXX
