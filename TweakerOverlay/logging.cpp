#include "pch.hpp"

#include "logging.hpp"

namespace
{
std::mutex logging_mutex;
char prefix_buffer[64];
char location_buffer[1024];
} // namespace

namespace
{
enum class LogLevel { Debug, Info, Warning, Error, Critical };
} // namespace

namespace
{
template<LogLevel level>
constexpr const char* LOG_ANSI;
template<>
constexpr const char* LOG_ANSI<LogLevel::Debug> = "1;34";       ///< Bold, blue text
template<>
constexpr const char* LOG_ANSI<LogLevel::Info> = "1;32";        ///< Bold, green text
template<>
constexpr const char* LOG_ANSI<LogLevel::Warning> = "1;33";     ///< Bold, yellow text
template<>
constexpr const char* LOG_ANSI<LogLevel::Error> = "1;31";       ///< Bold, red text
template<>
constexpr const char* LOG_ANSI<LogLevel::Critical> = "1;37;41"; ///< Bold, white text on red background
} // namespace

namespace
{
template<LogLevel level>
constexpr const char* LOG_PREFIX;
template<>
constexpr const char* LOG_PREFIX<LogLevel::Debug> = "[D]";
template<>
constexpr const char* LOG_PREFIX<LogLevel::Info> = "[I]";
template<>
constexpr const char* LOG_PREFIX<LogLevel::Warning> = "[W]";
template<>
constexpr const char* LOG_PREFIX<LogLevel::Error> = "[E]";
template<>
constexpr const char* LOG_PREFIX<LogLevel::Critical> = "[C]";
} // namespace

namespace
{
template<LogLevel level>
void log_message(const std::source_location& location, const std::string& message)
{
    std::lock_guard locker(logging_mutex);

    constexpr auto ansi = LOG_ANSI<level>;
    constexpr auto prefix = LOG_PREFIX<level>;
    std::snprintf(prefix_buffer, sizeof(prefix_buffer), "\033[%sm%s\033[0m", ansi, prefix);

    std::string path_file(std::filesystem::path(location.file_name()).filename().string());
    std::snprintf(location_buffer, sizeof(location_buffer), "%s:%lu", path_file.c_str(), static_cast<unsigned long>(location.line()));

    std::cerr << "\r" << prefix_buffer << " " << location_buffer << ": " << message << std::endl;
}
} // namespace

#ifndef NDEBUG
void logging::detail::debug(const std::source_location& location, const std::string& message)
{
    log_message<LogLevel::Debug>(location, message);
}
#endif

void logging::detail::info(const std::source_location& location, const std::string& message)
{
    log_message<LogLevel::Info>(location, message);
}

void logging::detail::warning(const std::source_location& location, const std::string& message)
{
    log_message<LogLevel::Warning>(location, message);
}

void logging::detail::error(const std::source_location& location, const std::string& message)
{
    log_message<LogLevel::Error>(location, message);
}

void logging::detail::critical(const std::source_location& location, const std::string& message)
{
    log_message<LogLevel::Critical>(location, message);
}
