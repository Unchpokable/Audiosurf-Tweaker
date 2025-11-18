#include "global.h"

#include "proc_check.hxx"

#define NOMINMAX
#include <Windows.h>

namespace
{
std::vector<std::string> enum_proc_dlls(std::uint64_t proc_id)
{
}
} // namespace

bool mmap::proc_has_external_deps(std::uint64_t proc_id, const std::vector<std::string>& dependent_dlls)
{
    return false;
}
