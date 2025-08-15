#pragma once

#ifndef NDEBUG
namespace logging::detail
{
void debug(const std::source_location& location, const std::string& message);
} // namespace logging::detail
#endif

namespace logging::detail
{
void info(const std::source_location& location, const std::string& message);
void warning(const std::source_location& location, const std::string& message);
void error(const std::source_location& location, const std::string& message);
void critical(const std::source_location& location, const std::string& message);
} // namespace logging::detail

#ifndef NDEBUG
namespace logging
{
template<typename... Args>
void debug(const std::source_location& location, const std::format_string<Args...> fmt, Args&&... args);
} // namespace logging
#endif

namespace logging
{
template<typename... Args>
void info(const std::source_location& location, const std::format_string<Args...> fmt, Args&&... args);
template<typename... Args>
void warning(const std::source_location& location, const std::format_string<Args...> fmt, Args&&... args);
template<typename... Args>
void error(const std::source_location& location, const std::format_string<Args...> fmt, Args&&... args);
template<typename... Args>
void critical(const std::source_location& location, const std::format_string<Args...> fmt, Args&&... args);
} // namespace logging

#ifndef NDEBUG
template<typename... Args>
void logging::debug(const std::source_location& location, const std::format_string<Args...> fmt, Args&&... args)
{
    logging::detail::debug(location, std::vformat(fmt.get(), std::make_format_args(args...)));
}
#endif

template<typename... Args>
void logging::info(const std::source_location& location, const std::format_string<Args...> fmt, Args&&... args)
{
    logging::detail::info(location, std::vformat(fmt.get(), std::make_format_args(args...)));
}

template<typename... Args>
void logging::warning(const std::source_location& location, const std::format_string<Args...> fmt, Args&&... args)
{
    logging::detail::warning(location, std::vformat(fmt.get(), std::make_format_args(args...)));
}

template<typename... Args>
void logging::error(const std::source_location& location, const std::format_string<Args...> fmt, Args&&... args)
{
    logging::detail::error(location, std::vformat(fmt.get(), std::make_format_args(args...)));
}

template<typename... Args>
void logging::critical(const std::source_location& location, const std::format_string<Args...> fmt, Args&&... args)
{
    logging::detail::critical(location, std::vformat(fmt.get(), std::make_format_args(args...)));
}

#ifndef NDEBUG
#define LOG_DEBUG(fmt, ...) logging::debug(std::source_location::current(), fmt, ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...)
#endif

#define LOG_INFO(fmt, ...)     logging::info(std::source_location::current(), fmt, ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...)  logging::warning(std::source_location::current(), fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)    logging::error(std::source_location::current(), fmt, ##__VA_ARGS__)
#define LOG_CRITICAL(fmt, ...) logging::critical(std::source_location::current(), fmt, ##__VA_ARGS__)
