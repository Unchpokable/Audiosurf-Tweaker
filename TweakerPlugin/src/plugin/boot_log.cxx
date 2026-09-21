#include "pch.hxx"

#include "plugin/boot_log.hxx"

namespace
{
// Past this the log is started over when the owning copy opens it - once per game launch, never while
// writing. A session's worth of lifecycle lines is a few kilobytes.
constexpr LONGLONG k_truncate_above_bytes = 1024 * 1024;

HANDLE g_file = INVALID_HANDLE_VALUE;
std::uint64_t g_process_created = 0;

std::uint64_t to_u64(const FILETIME& time) noexcept
{
    return (static_cast<std::uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
}

// Kernel32 only - see the header for why that matters here.
void ensure_directory(const std::wstring& directory) noexcept
{
    ::CreateDirectoryW(directory.c_str(), nullptr);
}

HANDLE open_file(const std::wstring& path, tw::plugin::boot_log::open_mode mode) noexcept
{
    constexpr DWORD k_share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;

    if(mode == tw::plugin::boot_log::open_mode::owner) {
        WIN32_FILE_ATTRIBUTE_DATA attributes {};
        if(::GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attributes) != FALSE) {
            const LONGLONG size = (static_cast<LONGLONG>(attributes.nFileSizeHigh) << 32) | attributes.nFileSizeLow;
            if(size > k_truncate_above_bytes) {
                const HANDLE truncated =
                    ::CreateFileW(path.c_str(), GENERIC_WRITE, k_share, nullptr, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                if(truncated != INVALID_HANDLE_VALUE) {
                    ::CloseHandle(truncated);
                }
            }
        }
    }

    // FILE_APPEND_DATA alone: every WriteFile lands at the current end of file atomically, so lines from
    // two threads - or from a guest copy writing into the owner's file - never overwrite each other.
    return ::CreateFileW(path.c_str(), FILE_APPEND_DATA, k_share, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
}
} // namespace

namespace tw::plugin::boot_log
{
void open(const std::filesystem::path& logs_dir, open_mode mode) noexcept
{
    if(g_file != INVALID_HANDLE_VALUE || logs_dir.empty()) {
        return;
    }

    FILETIME created {};
    FILETIME unused {};
    if(::GetProcessTimes(::GetCurrentProcess(), &created, &unused, &unused, &unused) != FALSE) {
        g_process_created = to_u64(created);
    }

    ensure_directory(logs_dir.parent_path().native());
    ensure_directory(logs_dir.native());

    g_file = open_file((logs_dir / L"TweakerPlugin.log").native(), mode);
}

void write_line(std::string_view text) noexcept
{
    if(g_file == INVALID_HANDLE_VALUE) {
        return;
    }

    // Seconds since the process was created rather than since the plugin loaded, so the numbers line up
    // with the Ф0 timeline in plugin-offline-mode.md.
    FILETIME now {};
    ::GetSystemTimePreciseAsFileTime(&now);
    const double seconds = g_process_created != 0 ? static_cast<double>(to_u64(now) - g_process_created) / 1.0e7 : 0.0;

    std::array<char, 896> line;
    const auto result = std::format_to_n(line.data(), line.size() - 2, "[+{:9.3f} s] [tid {:5}] {}", seconds, ::GetCurrentThreadId(), text);

    std::size_t length = static_cast<std::size_t>(result.out - line.data());
    length = std::min(length, line.size() - 2);
    line[length++] = '\r';
    line[length++] = '\n';

    // One WriteFile per line: with FILE_APPEND_DATA that is what keeps a line whole.
    DWORD written = 0;
    ::WriteFile(g_file, line.data(), static_cast<DWORD>(length), &written, nullptr);
}
} // namespace tw::plugin::boot_log
