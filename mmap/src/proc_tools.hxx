#include "static_string.hxx"

namespace mmap::tools
{
std::uint64_t get_proc_id(std::string_view proc_name) noexcept;
mmap::memory::StaticString<MAX_PATH> get_proc_root(std::string_view proc_name) noexcept;
} // namespace mmap::tools