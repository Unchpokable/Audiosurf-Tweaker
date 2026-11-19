#pragma once

#include <cstdint>

struct tweaker_metadata {
    const wchar_t* title;
    const wchar_t* artist;
    const wchar_t* album;
    std::int64_t duration_ms;
    std::int32_t bit_depth;
    std::int32_t sample_rate;
    std::int32_t bitrate;
};

// Status: 0 = success, 1 = invalid file/arguments, 2 = native exception, 3 = save failure.
// Strings belong to the returned snapshot, released exactly once by free_metadata.
extern "C" {
__declspec(dllexport) std::int32_t read_metadata(const wchar_t* path, tweaker_metadata* result, void** snapshot) noexcept;
__declspec(dllexport) void free_metadata(void* snapshot) noexcept;
__declspec(dllexport) std::int32_t write_title(const wchar_t* path, const wchar_t* title) noexcept;
}
