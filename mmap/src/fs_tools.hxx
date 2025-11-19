#pragma once

#include "static_string.hxx"

namespace mmap::fs
{
std::vector<mmap::memory::WinPathString> enum_files_with_extension(std::string_view directory, std::string_view extension);
} // namespace mmap::fs
