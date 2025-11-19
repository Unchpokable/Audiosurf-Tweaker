#include "global.h"

#include "fs_tools.hxx"

std::vector<mmap::memory::WinPathString> mmap::fs::enum_files_with_extension(std::string_view directory, std::string_view extension)
{
    std::filesystem::path path(directory);

    if(!std::filesystem::exists(directory)) {
        return {};
    }

    std::vector<mmap::memory::WinPathString> files;

    for(auto file : std::filesystem::directory_iterator(path)) {
        if(file.path().extension().string().ends_with(extension)) {
            files.emplace_back(file.path().string().c_str());
        }
    }

    return files;
}
