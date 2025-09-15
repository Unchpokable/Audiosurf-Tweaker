#ifndef RAW_IMAGE_HXX
#define RAW_IMAGE_HXX

#include "image_formats.hxx"

#include "Core_global.h"

namespace core
{
class CORE_EXPORT RawImage
{
public:
    enum CompressionLevel : int {
        Raw = 0,
        ZLib_Default = 6,
        Highest = 9
    };

public:
    static std::optional<RawImage> from_file(std::string_view path, CompressionLevel compression = ZLib_Default);

    void stream_insert(QDataStream& stream);
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
