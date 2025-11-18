namespace mmap
{
bool proc_has_external_deps(std::uint64_t proc_id, const std::vector<std::string>& dependent_dlls) noexcept;
}