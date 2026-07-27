#ifndef UULOG_HH
#define UULOG_HH
#pragma once

#if __cplusplus < 202002L && !defined(_MSC_VER)
#error "uulog requires C++20 or later"
#endif

#include <cstddef>

#ifdef UULOG_USE_FMTLIB
#include <fmt/format.h>
namespace uulog_fmt = fmt;
#else
#include <format>
namespace uulog_fmt = std;
#endif

#if defined(_MSC_VER)
#define UULOG_DLLEXPORT __declspec(dllexport)
#define UULOG_DLLIMPORT __declspec(dllimport)
#elif defined(__GNUC__) || defined(__clang__)
#define UULOG_DLLEXPORT __attribute__((visibility("default")))
#define UULOG_DLLIMPORT __attribute__((visibility("default")))
#else
#define UULOG_DLLEXPORT
#define UULOG_DLLIMPORT
#endif

#if defined(UULOG_DLL) && defined(UULOG_DLLPRIVATE)
#define UULOG_API UULOG_DLLEXPORT
#elif defined(UULOG_DLL)
#define UULOG_API UULOG_DLLIMPORT
#else
#define UULOG_API
#endif

namespace uulog
{
enum class Level {
    Info,     ///< Basic information messages
    Warning,  ///< Non-critical issues that may require attention
    Error,    ///< Errors that prevent normal operation but can be recovered from
    Critical, ///< Messages printed before fatal errors or crashes
    Debug,    ///< Debugging stuff, shouldn't be encountered when NDEBUG is defined
};
} // namespace uulog

namespace uulog
{
using Sink = void (*)(Level level, const char* file, unsigned long line, const char* message, std::size_t message_size);
} // namespace uulog

namespace uulog::builtin
{
UULOG_API void stderr_ansi(Level level, const char* file, unsigned long line, const char* message, std::size_t message_size);
UULOG_API void stderr_mono(Level level, const char* file, unsigned long line, const char* message, std::size_t message_size);
UULOG_API void stdout_ansi(Level level, const char* file, unsigned long line, const char* message, std::size_t message_size);
UULOG_API void stdout_mono(Level level, const char* file, unsigned long line, const char* message, std::size_t message_size);
} // namespace uulog::builtin

namespace uulog::builtin
{
#ifdef _WIN32
UULOG_API void win32_debugout(Level level, const char* file, unsigned long line, const char* message, std::size_t message_size);
#endif
} // namespace uulog::builtin

namespace uulog::detail
{
UULOG_API const char* level_ansi(Level level);
UULOG_API const char* level_string(Level level);
} // namespace uulog::detail

namespace uulog::detail
{
UULOG_API void info(const char* file, unsigned long line, const char* message, std::size_t message_size);
UULOG_API void warning(const char* file, unsigned long line, const char* message, std::size_t message_size);
UULOG_API void error(const char* file, unsigned long line, const char* message, std::size_t message_size);
UULOG_API void critical(const char* file, unsigned long line, const char* message, std::size_t message_size);
} // namespace uulog::detail

namespace uulog::detail
{
#ifndef NDEBUG
UULOG_API void debug(const char* file, unsigned long line, const char* message, std::size_t message_size);
#endif
} // namespace uulog::detail

namespace uulog
{
UULOG_API void add_sink(Sink sink);
UULOG_API void remove_sink(Sink sink);
} // namespace uulog

namespace uulog
{
template<typename... ArgsT>
void info(const char* file, unsigned long line, uulog_fmt::format_string<ArgsT...> fmt, ArgsT&&... args);
template<typename... ArgsT>
void warning(const char* file, unsigned long line, uulog_fmt::format_string<ArgsT...> fmt, ArgsT&&... args);
template<typename... ArgsT>
void error(const char* file, unsigned long line, uulog_fmt::format_string<ArgsT...> fmt, ArgsT&&... args);
template<typename... ArgsT>
void critical(const char* file, unsigned long line, uulog_fmt::format_string<ArgsT...> fmt, ArgsT&&... args);
} // namespace uulog

namespace uulog
{
#ifndef NDEBUG
template<typename... ArgsT>
void debug(const char* file, unsigned long line, uulog_fmt::format_string<ArgsT...> fmt, ArgsT&&... args);
#endif
} // namespace uulog

#define LOG_INFO(fmt, ...)     uulog::info(__FILE__, __LINE__, (fmt), ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...)  uulog::warning(__FILE__, __LINE__, (fmt), ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)    uulog::error(__FILE__, __LINE__, (fmt), ##__VA_ARGS__)
#define LOG_CRITICAL(fmt, ...) uulog::critical(__FILE__, __LINE__, (fmt), ##__VA_ARGS__)

#ifndef NDEBUG
#define LOG_DEBUG(fmt, ...) uulog::debug(__FILE__, __LINE__, (fmt), ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...) ((void)0)
#endif

template<typename... ArgsT>
void uulog::info(const char* file, unsigned long line, uulog_fmt::format_string<ArgsT...> fmt, ArgsT&&... args)
{
    auto message = uulog_fmt::vformat(fmt.get(), uulog_fmt::make_format_args(args...));
    uulog::detail::info(file, line, message.data(), message.size());
}

template<typename... ArgsT>
void uulog::warning(const char* file, unsigned long line, uulog_fmt::format_string<ArgsT...> fmt, ArgsT&&... args)
{
    auto message = uulog_fmt::vformat(fmt.get(), uulog_fmt::make_format_args(args...));
    uulog::detail::warning(file, line, message.data(), message.size());
}

template<typename... ArgsT>
void uulog::error(const char* file, unsigned long line, uulog_fmt::format_string<ArgsT...> fmt, ArgsT&&... args)
{
    auto message = uulog_fmt::vformat(fmt.get(), uulog_fmt::make_format_args(args...));
    uulog::detail::error(file, line, message.data(), message.size());
}

template<typename... ArgsT>
void uulog::critical(const char* file, unsigned long line, uulog_fmt::format_string<ArgsT...> fmt, ArgsT&&... args)
{
    auto message = uulog_fmt::vformat(fmt.get(), uulog_fmt::make_format_args(args...));
    uulog::detail::critical(file, line, message.data(), message.size());
}

#ifndef NDEBUG
template<typename... ArgsT>
void uulog::debug(const char* file, unsigned long line, uulog_fmt::format_string<ArgsT...> fmt, ArgsT&&... args)
{
    auto message = uulog_fmt::vformat(fmt.get(), uulog_fmt::make_format_args(args...));
    uulog::detail::debug(file, line, message.data(), message.size());
}
#endif

#endif // UULOG_HH
