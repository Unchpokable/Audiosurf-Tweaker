#include "precompiled.hxx"

#include "installer.hxx"

#include "logging.hxx"
#include "masks.hxx"
#include "pack_data.hxx"

#define HANDLE_RET(retval, shouldbe) \
    if((retval) != (shouldbe)) {     \
        return retval;               \
    }

namespace
{
core::InstallResult write_parts(const QString& destination, const QList<core::RawImage>& images)
{
    for(auto image : images) {
        auto stdstr_destination = destination.toStdString();
        auto result = image.write(stdstr_destination);
        if(!result) {
            LOG_WARNING("Failed to write: {}", stdstr_destination);
            return core::InstallResult::ReadWriteError;
        }
    }
}
} // namespace

core::InstallResult core::install_skin(const QString& destination, const PackData& data)
{
    QFileInfo info(destination);
    if(!info.exists()) {
        return InstallResult::DestinationNotExists;
    }

    if(!info.isDir()) {
        return InstallResult::DestinationNotDirectory;
    }

    auto install_result = write_parts(destination, data.required_parts);
    if(install_result != InstallResult::Ok) {
        return install_result;
    }

    return write_parts(destination, data.optional_parts);
}

core::InstallResult core::install_skin(const QString& destination, const PackData& data, InstallFilter filter)
{
    QFileInfo info(destination);
    if(!info.exists()) {
        return InstallResult::DestinationNotExists;
    }

    if(!info.isDir()) {
        return InstallResult::DestinationNotDirectory;
    }

    auto write_part = [](const QString& destination, core::RawImage& image) {
        auto stdstr_destination = destination.toStdString();
        auto result = image.write(stdstr_destination);
        if(!result) {
            LOG_WARNING("Failed to write: {}", stdstr_destination);
            return core::InstallResult::ReadWriteError;
        }

        return core::InstallResult::Ok;
    };

    auto handle_write_image = [write_part](
                                  const QString& destination, core::RawImage& image, const std::vector<const char*>& mask, bool should) {
        if(masks::contains_any(image.name(), masks::cliffs)) {
            if(should) {
                return write_part(destination, image);
            }
        }
    };

    for(auto image : data.required_parts) {
        core::InstallResult ret;

        ret = handle_write_image(destination, image, masks::cliffs, filter.install_cliffs);
        HANDLE_RET(ret, core::InstallResult::Ok);

        ret = handle_write_image(destination, image, masks::hits, filter.install_hits);
        HANDLE_RET(ret, core::InstallResult::Ok);

        ret = handle_write_image(destination, image, masks::particles, filter.install_particles);
        HANDLE_RET(ret, core::InstallResult::Ok);

        ret = handle_write_image(destination, image, masks::rings, filter.install_rings);
        HANDLE_RET(ret, core::InstallResult::Ok);

        ret = handle_write_image(destination, image, masks::tiles, filter.install_tiles);
        HANDLE_RET(ret, core::InstallResult::Ok);

        ret = handle_write_image(destination, image, masks::skyspheres, filter.install_skyspheres);
        HANDLE_RET(ret, core::InstallResult::Ok);
    }
}
