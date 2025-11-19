#pragma once

namespace mmap
{
enum MmapResult {
    MMAP_OK = 0,
    MMAP_ERR_NOMEM,
    MMAP_ERR_NOPERMS,
    MMAP_ERR_CANTWRITE,
    MMAP_ERR_SHLERR,
    MMAP_ERR_UNKNWN_ERR,
};
} // namespace mmap

namespace mmap
{
MmapResult mmap_load_dll(std::string_view dll_path, std::uint64_t proc_id) noexcept;
} // namespace mmap
