#pragma once

#include "Core_global.h"

namespace core
{
constexpr const char snapshot_extension[] = "hinf";
} // namespace core

namespace core
{
// Wrapper for 128-bit unsigned hashes to avoid including <xxhash> everywhere
struct Unsigned128 {
    std::uint64_t low64 {};
    std::uint64_t high64 {};
};

struct FileHash {
    Unsigned128 hash {};
    QString file_name {};
    bool valid { true };
};

struct ContentSnapshot {
    QList<FileHash> hashes {};
    QByteArray unique_name {};
};
} // namespace core

namespace core
{
enum class ContentVerifyResult {
    Ok,
    FilenameMissmatch,
    HashMissmatch,
    CountMissmatch,
};
} // namespace core

namespace core::detail
{
bool u128_same(const Unsigned128& lhs, const Unsigned128& rhs);
} // namespace core::detail

namespace core
{
CORE_EXPORT ContentVerifyResult verify(const QString& destination, ContentSnapshot snapshot);
CORE_EXPORT ContentSnapshot make_snapshot(const QString& destination, const QByteArray& unique_name);
CORE_EXPORT bool write_snapshot(const QString& destination, ContentSnapshot snapshot);
CORE_EXPORT std::optional<ContentSnapshot> load_snapshot(const QString& destination);
} // namespace core