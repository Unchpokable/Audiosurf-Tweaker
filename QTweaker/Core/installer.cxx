#include "precompiled.hxx"

#include "installer.hxx"

#include "logging.hxx"
#include "masks.hxx"

namespace
{
std::optional<XXH128_hash_t> file_hash_128(const QString& path)
{
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)) {
        LOG_WARNING("Unable to open: {}", path.toStdString());
        return std::nullopt;
    }

    XXH3_state_t* state = XXH3_createState();
    XXH3_128bits_reset(state);

    constexpr qint64 chunk_size = 64 * 1024;
    while(!file.atEnd()) {
        auto chunk = file.read(chunk_size);
        XXH3_128bits_update(state, chunk.constData(), chunk.size());
    }

    XXH128_hash_t hash = XXH3_128bits_digest(state);
    XXH3_freeState(state);

    return hash;
}
} // namespace

bool core::install_skin(QStringView destination, const PackData& data)
{
    return false;
}

bool core::install_skin(QStringView destination, const PackData& data, InstallFilter filter)
{
    return false;
}

bool core::verify(QStringView destination, ContentSnapshot snapshot)
{
    return false;
}

core::ContentSnapshot core::make_snapshot(QStringView destination, const QByteArray& unique_name)
{
    return {};
}

std::optional<core::ContentSnapshot> core::load_snapshot(QStringView destination)
{
    return std::nullopt;
}
