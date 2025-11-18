#pragma once

namespace mmap
{
bool mmap_load_dll(std::string_view dll_path, std::uint64_t proc_id) noexcept;
} // namespace mmap
