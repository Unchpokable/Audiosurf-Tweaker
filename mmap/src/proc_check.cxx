#include "global.h"

#include "proc_check.hxx"

#include "static_string.hxx"

namespace
{
std::vector<mmap::memory::StaticString<MAX_PATH>> enum_proc_dlls(std::uint64_t proc_id) noexcept
{
}
} // namespace

bool mmap::proc_has_external_deps(std::uint64_t proc_id, const std::vector<std::string>& dependent_dlls) noexcept
{
    return false;
}
