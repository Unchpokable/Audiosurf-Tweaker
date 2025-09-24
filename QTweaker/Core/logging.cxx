#include "precompiled.hxx"

#include "logging.hxx"

namespace
{
std::mutex mutex;
std::vector<logging::SinkFunction> sinks_vector;
} // namespace

namespace
{
template<logging::Level level>
void common_log(const std::source_location& location, const std::string& message)
{
    for(const auto& sink : sinks_vector) {
        sink(level, location, message);
    }
}
} // namespace

void logging::sinks::stderr_ansi(Level level, const std::source_location& location, const std::string& message)
{
    thread_local char prefix_buffer[64];
    thread_local char location_buffer[1024];

    const char* level_str = nullptr;
    const char* ansi = nullptr;

    switch(level) {
        case Level::Info:
            level_str = "inf";
            ansi = "1;32"; ///< Bold, green text
            break;

        case Level::Warning:
            level_str = "wrn";
            ansi = "1;33"; ///< Bold, yellow text
            break;

        case Level::Error:
            level_str = "err";
            ansi = "1;31"; ///< Bold, red text
            break;

        case Level::Critical:
            level_str = "crt";
            ansi = "1;37;41"; ///< Bold, white text on red background
            break;

        case Level::Debug:
            level_str = "dbg";
            ansi = "1;34"; ///< Bold, blue text
            break;
    }

    std::snprintf(prefix_buffer, sizeof(prefix_buffer), "\033[%sm[%s]\033[0m", ansi, level_str);

    std::string path_file(std::filesystem::path(location.file_name()).filename().string());
    std::snprintf(location_buffer, sizeof(location_buffer), "%s:%lu", path_file.c_str(), static_cast<unsigned long>(location.line()));

    std::lock_guard lock(mutex);
    std::cerr << "\r" << prefix_buffer << " " << location_buffer << ": " << message << std::endl;
}

void logging::add_sink(SinkFunction sink)
{
    std::lock_guard lock(mutex);
    sinks_vector.push_back(sink);
}

void logging::remove_sink(SinkFunction sink)
{
    std::lock_guard lock(mutex);
    std::erase(sinks_vector, sink);
}

void logging::detail::info(const std::source_location& location, const std::string& message)
{
    common_log<Level::Info>(location, message);
}

void logging::detail::warning(const std::source_location& location, const std::string& message)
{
    common_log<Level::Warning>(location, message);
}

void logging::detail::error(const std::source_location& location, const std::string& message)
{
    common_log<Level::Error>(location, message);
}

void logging::detail::critical(const std::source_location& location, const std::string& message)
{
    common_log<Level::Critical>(location, message);
}

#ifndef NDEBUG
void logging::detail::debug(const std::source_location& location, const std::string& message)
{
    common_log<Level::Debug>(location, message);
}
#endif
