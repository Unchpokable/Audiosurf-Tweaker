#include "precompiled.hxx"

#include "snapshot.hxx"

#include "logging.hxx"

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

bool core::detail::u128_same(const Unsigned128& lhs, const Unsigned128& rhs)
{
    return lhs.high64 == rhs.high64 && lhs.low64 == rhs.low64;
}

core::ContentVerifyResult core::verify(const QString& destination, ContentSnapshot snapshot)
{
    auto actual_snapshot = make_snapshot(destination, "compare_template");

    auto same_sized = snapshot.hashes.count() == actual_snapshot.hashes.count();
    if(!same_sized) {
        return ContentVerifyResult::CountMissmatch;
    }

    for(qsizetype i { 0 }; i < actual_snapshot.hashes.count(); ++i) {
        auto actual_hash = actual_snapshot.hashes.at(i);
        auto compared_hash = snapshot.hashes.at(i);

        if(!core::detail::u128_same(actual_hash.hash, compared_hash.hash)) {
            return ContentVerifyResult::HashMissmatch;
        }

        if(actual_hash.file_name != compared_hash.file_name) {
            return ContentVerifyResult::FilenameMissmatch;
        }
    }

    return ContentVerifyResult::Ok;
}

core::ContentSnapshot core::make_snapshot(const QString& destination, const QByteArray& unique_name)
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

    QThreadPool thread_pool;
    thread_pool.setMaxThreadCount(QThread::idealThreadCount());

    auto future = QtConcurrent::mapped(&thread_pool, files, [](const QString& path) {
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

    auto raw_path = destination + "/" + QString("%1.%2").arg("current").arg(snapshot_extension);
    auto path = QDir::cleanPath(QDir::toNativeSeparators(raw_path));

    QFile file(path);
    if(!file.open(QIODevice::OpenModeFlag::ReadOnly)) {
        LOG_WARNING("Unable to open device to write!: {}", destination.toStdString());
        return false;
    }

    QDataStream stream(&file);

    stream << snapshot.unique_name;
    stream << snapshot.hashes.count();

    for(auto hash : snapshot.hashes) {
        stream << hash.file_name;
        stream << static_cast<quint64>(hash.hash.low64);
        stream << static_cast<quint64>(hash.hash.high64);

        if(stream.status() != QDataStream::Status::Ok) {
            LOG_WARNING("Writer died! Status code: {}", stream.status());
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
}
