#ifndef INSTALLER_H
#define INSTALLER_H

#include "Core_global.h"

namespace core
{
struct PackData;
} // namespace core

namespace core
{
struct InstallFilter {
    bool install_tiles;
    bool install_rings;
    bool install_hits;
    bool install_particles;
    bool install_skyspheres;
};
} // namespace core

namespace core
{
CORE_EXPORT bool install_skin(const QString& destination, const PackData& data);
CORE_EXPORT bool install_skin(const QString& destination, const PackData& data, InstallFilter filter);
} // namespace core

#endif // INSTALLER_H
