#pragma once

namespace logging
{
enum class Level : unsigned int {
    Info,     ///< Basic information messages
    Warning,  ///< Non-critical issues that may require attention
    Error,    ///< Errors that prevent normal operation but can be recovered from
    Critical, ///< Messages printed before fatal errors or crashes
    Debug,    ///< Debugging stuff, shouldn't be encountered when NDEBUG is defined
};
} // namespace logging

namespace logging
{
using SinkFunction = void (*)(Level level, const std::source_location& location, const std::string& message);
} // namespace logging

namespace logging::sinks
{
void stderr_ansi(Level level, const std::source_location& location, const std::string& message);
} // namespace logging::sinks

namespace logging::detail
{
void info(const std::source_location& location, const std::string& message);
void warning(const std::source_location& location, const std::string& message);
void error(const std::source_location& location, const std::string& message);
void critical(const std::source_location& location, const std::string& message);
} // namespace logging::detail

namespace logging::detail
{
#ifndef NDEBUG
void debug(const std::source_location& location, const std::string& message);
#endif
} // namespace logging::detail

namespace logging
{
void add_sink(SinkFunction sink);
void remove_sink(SinkFunction sink);
} // namespace logging

namespace logging
{
template<typename... ArgsT>
void info(const std::source_location& location, std::format_string<ArgsT...> fmt, ArgsT&&... args);
template<typename... ArgsT>
void warning(const std::source_location& location, std::format_string<ArgsT...> fmt, ArgsT&&... args);
template<typename... ArgsT>
void error(const std::source_location& location, std::format_string<ArgsT...> fmt, ArgsT&&... args);
template<typename... ArgsT>
void critical(const std::source_location& location, std::format_string<ArgsT...> fmt, ArgsT&&... args);
} // namespace logging

namespace logging
{
#ifndef NDEBUG
template<typename... ArgsT>
void debug(const std::source_location& location, std::format_string<ArgsT...> fmt, ArgsT&&... args);
#endif
} // namespace logging

template<typename... ArgsT>
void logging::info(const std::source_location& location, std::format_string<ArgsT...> fmt, ArgsT&&... args)
{
    logging::detail::info(location, std::vformat(fmt.get(), std::make_format_args(args...)));
}

template<typename... ArgsT>
void logging::warning(const std::source_location& location, std::format_string<ArgsT...> fmt, ArgsT&&... args)
{
    logging::detail::warning(location, std::vformat(fmt.get(), std::make_format_args(args...)));
}

template<typename... ArgsT>
void logging::error(const std::source_location& location, std::format_string<ArgsT...> fmt, ArgsT&&... args)
{
    logging::detail::error(location, std::vformat(fmt.get(), std::make_format_args(args...)));
}

template<typename... ArgsT>
void logging::critical(const std::source_location& location, std::format_string<ArgsT...> fmt, ArgsT&&... args)
{
    logging::detail::critical(location, std::vformat(fmt.get(), std::make_format_args(args...)));
}

#ifndef NDEBUG
template<typename... ArgsT>
void logging::debug(const std::source_location& location, std::format_string<ArgsT...> fmt, ArgsT&&... args)
{
    logging::detail::debug(location, std::vformat(fmt.get(), std::make_format_args(args...)));
}
#endif

#define LOG_INFO(fmt, ...)     logging::info(std::source_location::current(), fmt, ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...)  logging::warning(std::source_location::current(), fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)    logging::error(std::source_location::current(), fmt, ##__VA_ARGS__)
#define LOG_CRITICAL(fmt, ...) logging::critical(std::source_location::current(), fmt, ##__VA_ARGS__)

#ifndef NDEBUG
#define LOG_DEBUG(fmt, ...) logging::debug(std::source_location::current(), fmt, ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...)
#endif
