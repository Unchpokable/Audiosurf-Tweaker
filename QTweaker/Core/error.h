#ifndef ERROR_H
#define ERROR_H

#define tweaker_noreturn [[noreturn]]

namespace core
{
template<typename T>
concept std_exception_based = std::derived_from<T, std::exception>;
}

namespace core
{

enum Rank
{
    Fatal,
    Severe,
    Moderate,
    Minor,
    Trivial
};

struct Error {
    Rank error_rank;

    QString short_description;
    QString detailed_description;

    std::unique_ptr<std::exception> inner_exception;
};
}

namespace core
{
template<std_exception_based Err = std::exception>
Error make_error(Rank rank, const QString& what, const QString& details = QString());

template<std_exception_based Err = std::exception, typename... Args>
Error format_error(Rank rank, const QString& what, const std::string& defails, Args... format_args);
}

namespace core
{
tweaker_noreturn inline void panic(const Error& error)
{
    throw *error.inner_exception;
}
}

namespace core
{
template<std_exception_based Err>
Error make_error(Rank rank, const QString& what, const QString& details)
{
    Error error;
    error.error_rank = rank;

    error.short_description = what;
    error.detailed_description = details;

    auto exception = std::make_unique<Err>(std::format("{} : {}", what.toStdString(), details.toStdString()));

    error.inner_exception = exception;

    return error;
}

template<std_exception_based Err, typename... Args>
Error format_error(Rank rank, const QString& what, const std::string& details, Args... format_args)
{
    auto formatted_details = std::vformat(details, std::make_format_args(format_args...));
    auto qs_details = QString::fromStdString(formatted_details);

    return make_error(rank, what, qs_details);
}
}

#endif // ERROR_H
