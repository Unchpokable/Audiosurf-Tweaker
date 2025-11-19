#include "static_string.hxx"

namespace mmap
{
bool proc_has_external_deps(std::uint64_t proc_id, const std::vector<mmap::memory::StaticString<MAX_PATH>>& dependent_dlls) noexcept;
}