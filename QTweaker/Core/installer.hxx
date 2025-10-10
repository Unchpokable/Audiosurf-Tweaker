#ifndef INSTALLER_H
#define INSTALLER_H

#include "Core_global.h"

namespace core
{
struct PackData;
}

namespace core
{
struct InstallFilter
{
    bool install_tiles;
    bool install_rings;
    bool install_hits;
    bool install_particles;
    bool install_skyspheres;
};
}

namespace core
{
struct FileHash
{
    std::uint64_t hash;
    QString file_name;
};

struct ContentSnapshot
{
    QList<FileHash> hashes;
    QByteArray unique_name;
};
}

namespace core
{
CORE_EXPORT bool install_skin(QStringView destination, const PackData& data);
CORE_EXPORT bool install_skin(QStringView destination, const PackData& data, InstallFilter filter);
}

namespace core
{
CORE_EXPORT bool verify(QStringView destination, ContentSnapshot snapshot);
CORE_EXPORT ContentSnapshot make_snapshot(QStringView destination, const QByteArray& unique_name);
CORE_EXPORT bool write_snapshot(QStringView destination, ContentSnapshot snapshot);
CORE_EXPORT std::optional<ContentSnapshot> load_snapshot(QStringView destination);
}

#endif // INSTALLER_H
