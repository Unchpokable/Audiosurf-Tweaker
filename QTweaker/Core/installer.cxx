#include "precompiled.hxx"

#include "installer.hxx"

#include "logging.hxx"
#include "masks.hxx"
#include "pack_data.hxx"

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
    return InstallResult::GenericFailure;
}
