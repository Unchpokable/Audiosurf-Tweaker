#include "precompiled.hxx"

#include "error.h"

core::Error make_error(
    core::Rank rank, const QString& what, const QString& details)
{
    core::Error error;
    error.error_rank = rank;

    error.short_description = what;
    error.detailed_description = details;

    return error;
}
