#include "precompiled.hxx"

#include "snapshot.hxx"

#include "logging.hxx"

namespace
{
class xxHashRaiiWrapper {
public:
    xxHashRaiiWrapper() : m_state(XXH3_createState())
    {
    }
    ~xxHashRaiiWrapper()
    {
        if(m_state) {
            XXH3_freeState(m_state);
            m_state = nullptr;
        }
    }

    xxHashRaiiWrapper(const xxHashRaiiWrapper&) = delete;
    xxHashRaiiWrapper& operator=(const xxHashRaiiWrapper&) = delete;

    operator XXH3_state_t*() const
    {
        return m_state;
    }

private:
    XXH3_state_t* m_state;
};

std::optional<XXH128_hash_t> file_hash_128(const QString& path)
{
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)) {
        LOG_WARNING("Unable to open: {}", path.toStdString());
        return std::nullopt;
    }

    xxHashRaiiWrapper state;
    XXH3_128bits_reset(state);

    constexpr qint64 chunk_size = 64 * 1024;
    while(!file.atEnd()) {
        auto chunk = file.read(chunk_size);
        XXH3_128bits_update(state, chunk.constData(), chunk.size());
    }

    XXH128_hash_t hash = XXH3_128bits_digest(state);

    return hash;
}
} // namespace

bool core::detail::u128_same(const Unsigned128& lhs, const Unsigned128& rhs)
{
    return lhs.high64 == rhs.high64 && lhs.low64 == rhs.low64;
}

core::ContentVerifyResult core::verify(const QString& destination, ContentSnapshot snapshot)
{
    auto actual_snapshot = make_snapshot(destination, "compare_template");

    if(actual_snapshot) {
        return ContentVerifyResult::HashMissmatch;
    }

    auto same_sized = snapshot.hashes.count() == actual_snapshot->hashes.count();
    if(!same_sized) {
        return ContentVerifyResult::CountMissmatch;
    }

    for(auto& hash : actual_snapshot->hashes) {
        auto saved_hash = std::ranges::find_if(snapshot.hashes, [hash](const core::FileHash& sv_hash) {
            return hash.file_name == sv_hash.file_name;
        });

        if(saved_hash == std::ranges::end(snapshot.hashes)) {
            return ContentVerifyResult::FilenameMissmatch;
        }

        if(!core::detail::u128_same(hash.hash, saved_hash->hash)) {
            return ContentVerifyResult::HashMissmatch;
        }
    }

    return ContentVerifyResult::Ok;
}

std::optional<core::ContentSnapshot> core::make_snapshot(const QString& destination, const QByteArray& unique_name)
{
    QFileInfo info(destination);
    if(!info.isDir()) {
        LOG_WARNING("Given location is not a directory! {}", destination.toStdString());
        return {};
    }

    QDirIterator iterator(destination);
    QStringList files;
    while(iterator.hasNext()) {
        auto file = iterator.next();
        QFileInfo file_info(file);
        if(file_info.isDir()) {
            continue;
        }
        if(file_info.suffix() == snapshot_extension) { // skin .hinf files with stored state
            continue;
        }

        files.append(file);
    }

    auto future = QtConcurrent::mapped(QThreadPool::globalInstance(), files, [](const QString& path) {
        auto hash = file_hash_128(path);
        FileHash result;
        if(hash) {
            result.hash.low64 = hash->low64;
            result.hash.high64 = hash->high64;

            result.file_name = path;
            result.valid = true;
        }
        else {
            result.valid = false;
        }

        return result;
    });

    auto result = future.results();

    for(auto& hash : result) {
        if(hash.valid == false) {
            LOG_WARNING("Hashing {} was failed!", destination.toStdString());
            return std::nullopt;
        }
    }

    ContentSnapshot snapshot;
    snapshot.hashes = result;
    snapshot.unique_name = unique_name;

    return snapshot;
}

bool core::write_snapshot(const QString& destination, ContentSnapshot snapshot)
{
    QFileInfo info(destination);
    if(!info.exists()) {
        LOG_WARNING("Path {} is not exists!!!", destination.toStdString());
        return false;
    }

    if(!info.isDir()) {
        LOG_WARNING("Path {} is not a directory!!!", destination.toStdString());
        return false;
    }

    auto raw_path = destination + "/" + QString("%1.%2").arg(snapshot_filename).arg(snapshot_extension);
    auto path = QDir::cleanPath(QDir::toNativeSeparators(raw_path));

    QFile file(path);
    if(!file.open(QIODevice::OpenModeFlag::WriteOnly)) {
        LOG_WARNING("Unable to open device to write!: {}", destination.toStdString());
        return false;
    }

    QDataStream stream(&file);

    stream << snapshot.unique_name;
    if(stream.status() != QDataStream::Status::Ok) {
        LOG_WARNING("Failed to write unique_name to snapshot. Status code: {}", static_cast<int>(stream.status()));
        file.close();
        return false;
    }

    stream << snapshot.hashes.count();
    if(stream.status() != QDataStream::Status::Ok) {
        LOG_WARNING("Failed to write hash count to snapshot. Status code: {}", static_cast<int>(stream.status()));
        file.close();
        return false;
    }

    for(qsizetype i = 0; i < snapshot.hashes.count(); ++i) {
        const auto& hash = snapshot.hashes.at(i);

        stream << hash.file_name;
        if(stream.status() != QDataStream::Status::Ok) {
            LOG_WARNING("Failed to write file_name at index {}. Status code: {}", i, static_cast<int>(stream.status()));
            file.close();
            return false;
        }

        stream << static_cast<quint64>(hash.hash.low64);
        if(stream.status() != QDataStream::Status::Ok) {
            LOG_WARNING("Failed to write low64 at index {}. Status code: {}", i, static_cast<int>(stream.status()));
            file.close();
            return false;
        }

        stream << static_cast<quint64>(hash.hash.high64);
        if(stream.status() != QDataStream::Status::Ok) {
            LOG_WARNING("Failed to write high64 at index {}. Status code: {}", i, static_cast<int>(stream.status()));
            file.close();
            return false;
        }
    }

    file.close();

    return true;
}

std::optional<core::ContentSnapshot> core::load_snapshot(const QString& destination)
{
    QFileInfo info(destination);
    if(!info.exists()) {
        LOG_WARNING("Path {} is not exists!!!", destination.toStdString());
        return std::nullopt;
    }

    if(!info.isDir()) {
        LOG_WARNING("Path {} is not a directory!!!", destination.toStdString());
        return std::nullopt;
    }

    auto raw_path = destination + "/" + QString("%1.%2").arg(snapshot_filename).arg(snapshot_extension);
    auto path = QDir::cleanPath(QDir::toNativeSeparators(raw_path));

    QFileInfo file_info(path);
    if(!file_info.exists()) {
        LOG_WARNING("Snapshot file does not exist: {}", path.toStdString());
        return std::nullopt;
    }

    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)) {
        LOG_WARNING("Unable to open snapshot file for reading: {}", path.toStdString());
        return std::nullopt;
    }

    QDataStream stream(&file);

    ContentSnapshot snapshot;

    stream >> snapshot.unique_name;
    if(stream.status() != QDataStream::Status::Ok) {
        LOG_WARNING("Failed to read unique_name from snapshot. Status code: {}", static_cast<int>(stream.status()));
        file.close();
        return std::nullopt;
    }

    qsizetype hash_count = 0;
    stream >> hash_count;
    if(stream.status() != QDataStream::Status::Ok) {
        LOG_WARNING("Failed to read hash count from snapshot. Status code: {}", static_cast<int>(stream.status()));
        file.close();
        return std::nullopt;
    }

    snapshot.hashes.reserve(hash_count);

    for(qsizetype i = 0; i < hash_count; ++i) {
        FileHash hash;
        hash.valid = true;

        stream >> hash.file_name;
        if(stream.status() != QDataStream::Status::Ok) {
            LOG_WARNING("Failed to read file_name at index {}. Status code: {}", i, static_cast<int>(stream.status()));
            file.close();
            return std::nullopt;
        }

        quint64 low64, high64;
        stream >> low64;
        if(stream.status() != QDataStream::Status::Ok) {
            LOG_WARNING("Failed to read low64 at index {}. Status code: {}", i, static_cast<int>(stream.status()));
            file.close();
            return std::nullopt;
        }

        stream >> high64;
        if(stream.status() != QDataStream::Status::Ok) {
            LOG_WARNING("Failed to read high64 at index {}. Status code: {}", i, static_cast<int>(stream.status()));
            file.close();
            return std::nullopt;
        }

        hash.hash.low64 = low64;
        hash.hash.high64 = high64;

        snapshot.hashes.append(hash);
    }

    file.close();

    return snapshot;
}
