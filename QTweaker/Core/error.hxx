#ifndef ERROR_H
#define ERROR_H

#define tweaker_noreturn [[noreturn]]

#include "Core_global.h"

namespace core
{

template<typename T>
concept std_exception_based = std::derived_from<T, std::exception> && std::constructible_from<T, const char*>;

}

namespace core
{

enum Rank
{
    Fatal,
    Severe,
    Moderate,
    Minor,
    Trivial,
    Nothing,
};

struct Error
{
    Rank error_rank { Nothing };

    QString short_description;
    QString detailed_description;
};

}

namespace core::detail
{
CORE_EXPORT Error make_error(Rank rank, const QString& what, const QString& details = QString());
}

namespace core
{

template<typename... Args>
Error make_error(Rank rank, const QString& what, const std::format_string<Args...> details, Args... format_args);

inline bool is_fine(const core::Error& err)
{
    return err.error_rank == Nothing;
}

}

namespace core
{

template<std_exception_based Err>
tweaker_noreturn inline void panic(
    const Error& error)
{
    auto std_message = std::format("{}: {}", error.short_description.toStdString(), error.detailed_description.toStdString());
    throw Err(std_message);
}

} // namespace core

namespace core
{

template<typename... Args>
Error make_error(
    Rank rank, const QString& what, const std::format_string<Args...> details, Args... format_args)
{
    auto formatted_details = std::vformat(details.get(), std::make_format_args(format_args...));
    auto qs_details = QString::fromStdString(formatted_details);

    return core::detail::make_error(rank, what, qs_details);
}

}

#endif
