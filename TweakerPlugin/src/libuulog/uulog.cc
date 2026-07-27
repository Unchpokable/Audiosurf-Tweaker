#include "uulog.hh"

#include <cassert>
#include <filesystem>
#include <mutex>
#include <string_view>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static std::mutex s_mutex;
static std::vector<uulog::Sink> s_sinks;

template<uulog::Level LevelValue>
static void common_log(const char* file, unsigned long line, const char* message, std::size_t message_size)
{
    std::lock_guard locker(s_mutex);

    for(const auto& sink : s_sinks) {
        sink(LevelValue, file, line, message, message_size);
    }
}

static void builtin_sink_ansi(FILE* fd, uulog::Level level, const char* file, unsigned long line, const char* message, std::size_t message_size)
{
    thread_local char prefix_buffer[64];
    thread_local char location_buffer[1024];

    assert(fd);
    assert(file);
    assert(message);

    auto level_str = uulog::detail::level_string(level);
    auto level_ansi = uulog::detail::level_ansi(level);
    std::snprintf(prefix_buffer, sizeof(prefix_buffer), "\033[%sm[%s]\033[0m", level_ansi, level_str);

    std::string path_file(std::filesystem::path(file).filename().string());
    std::snprintf(location_buffer, sizeof(location_buffer), "%s:%lu", path_file.c_str(), line);

    std::fprintf(fd, "%s %s: %.*s\r\n", prefix_buffer, location_buffer, static_cast<int>(message_size), message);
}

static void builtin_sink_mono(FILE* fd, uulog::Level level, const char* file, unsigned long line, const char* message, std::size_t message_size)
{
    thread_local char prefix_buffer[64];
    thread_local char location_buffer[1024];

    assert(fd);
    assert(file);
    assert(message);

    auto level_str = uulog::detail::level_string(level);
    std::snprintf(prefix_buffer, sizeof(prefix_buffer), "[%s]", level_str);

    std::string path_file(std::filesystem::path(file).filename().string());
    std::snprintf(location_buffer, sizeof(location_buffer), "%s:%lu", path_file.c_str(), line);

    std::fprintf(fd, "%s %s: %.*s\r\n", prefix_buffer, location_buffer, static_cast<int>(message_size), message);
}

UULOG_API void uulog::builtin::stderr_ansi(Level level, const char* file, unsigned long line, const char* message, std::size_t message_size)
{
    assert(file);
    assert(message);

    builtin_sink_ansi(stderr, level, file, line, message, message_size);
}

UULOG_API void uulog::builtin::stderr_mono(Level level, const char* file, unsigned long line, const char* message, std::size_t message_size)
{
    assert(file);
    assert(message);

    builtin_sink_mono(stderr, level, file, line, message, message_size);
}

UULOG_API void uulog::builtin::stdout_ansi(Level level, const char* file, unsigned long line, const char* message, std::size_t message_size)
{
    assert(file);
    assert(message);

    builtin_sink_ansi(stdout, level, file, line, message, message_size);
}

UULOG_API void uulog::builtin::stdout_mono(Level level, const char* file, unsigned long line, const char* message, std::size_t message_size)
{
    assert(file);
    assert(message);

    builtin_sink_mono(stdout, level, file, line, message, message_size);
}

#ifdef _WIN32
UULOG_API void uulog::builtin::win32_debugout(Level level, const char* file, unsigned long line, const char* message, std::size_t message_size)
{
    assert(file);
    assert(message);

    thread_local char buffer[2048];

    auto level_str = uulog::detail::level_string(level);
    std::snprintf(buffer, sizeof(buffer), "[%s] %s:%lu: %.*s\r\n", level_str, file, line, static_cast<int>(message_size), message);
    OutputDebugStringA(buffer);
}
#endif

UULOG_API const char* uulog::detail::level_ansi(uulog::Level level)
{
    switch(level) {
        case uulog::Level::Info:
            return "1;32"; // bold green

        case uulog::Level::Warning:
            return "1;33"; // bold yellow

        case uulog::Level::Error:
            return "1;31"; // bold red

        case uulog::Level::Critical:
            return "1;41"; // bold white on red background

        case uulog::Level::Debug:
            return "1;34"; // bold blue

        default:
            return "1;35"; // bold magenta
    }
}

UULOG_API const char* uulog::detail::level_string(Level level)
{
    switch(level) {
        case uulog::Level::Info:
            return "info";

        case uulog::Level::Warning:
            return "warning";

        case uulog::Level::Error:
            return "error";

        case uulog::Level::Critical:
            return "critical";

        case uulog::Level::Debug:
            return "debug";

        default:
            return "unknown";
    }
}

UULOG_API void uulog::detail::info(const char* file, unsigned long line, const char* message, std::size_t message_size)
{
    assert(file);
    assert(message);

    common_log<uulog::Level::Info>(file, line, message, message_size);
}

UULOG_API void uulog::detail::warning(const char* file, unsigned long line, const char* message, std::size_t message_size)
{
    assert(file);
    assert(message);

    common_log<uulog::Level::Warning>(file, line, message, message_size);
}

UULOG_API void uulog::detail::error(const char* file, unsigned long line, const char* message, std::size_t message_size)
{
    assert(file);
    assert(message);

    common_log<uulog::Level::Error>(file, line, message, message_size);
}

UULOG_API void uulog::detail::critical(const char* file, unsigned long line, const char* message, std::size_t message_size)
{
    assert(file);
    assert(message);

    common_log<uulog::Level::Critical>(file, line, message, message_size);
}

#ifndef NDEBUG
UULOG_API void uulog::detail::debug(const char* file, unsigned long line, const char* message, std::size_t message_size)
{
    assert(file);
    assert(message);

    common_log<uulog::Level::Debug>(file, line, message, message_size);
}
#endif

UULOG_API void uulog::add_sink(Sink sink)
{
    assert(sink);

    std::lock_guard locker(s_mutex);

    s_sinks.push_back(sink);
}

UULOG_API void uulog::remove_sink(Sink sink)
{
    assert(sink);

    std::lock_guard locker(s_mutex);

    std::erase(s_sinks, sink);
}
