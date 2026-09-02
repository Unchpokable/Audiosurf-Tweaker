# Vendored yyjson (https://github.com/ibireme/yyjson), MIT, v0.12.0.
#
# The JSON reader for the .sky package manifest. Chosen because it is ANSI C: there are no
# exceptions to disable and no C++ error-handling style to reconcile with this project's, only
# return codes and a yyjson_read_err out-parameter - which is exactly the shape CLAUDE.md asks for.
# Two files, no dependencies, and vendored rather than fetched so a configure needs no network.
#
# Comments and trailing commas are not JSON, they are read flags (YYJSON_READ_ALLOW_COMMENTS /
# _TRAILING_COMMAS) that sky_package passes. A sky author annotating what a parameter means is the
# normal case, so they are on.
add_library(libyyjson STATIC
    yyjson.c
)

target_include_directories(libyyjson PUBLIC
    "${CMAKE_SOURCE_DIR}/src"
)

if(MSVC)
    # Third-party; keep the noise down without editing vendor sources.
    target_compile_options(libyyjson PRIVATE /W0)
endif()
