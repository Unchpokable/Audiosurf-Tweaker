# Vendored uulog (see LICENCE) - compiled once for the whole tree, like libstb.
#
# The sources keep their upstream .cc/.hh extensions and upstream formatting on purpose: vendored
# code is not restyled to this project's .cxx/.hxx convention (see CLAUDE.md, "Vendored-код ...
# ТРОГАТЬ ЗАПРЕЩЕНО").
add_library(libuulog STATIC
    uulog.cc
)

target_include_directories(libuulog PUBLIC
    "${CMAKE_SOURCE_DIR}/src"
)

if(MSVC)
    # Vendored: keep warning noise out without editing upstream sources. /FI forces the <cstdio>
    # that uulog.cc relies on transitively (it uses std::fprintf/std::snprintf/FILE but includes
    # only <filesystem>) - a build fix that would otherwise require touching the vendor file.
    target_compile_options(libuulog PRIVATE /W0 /FIcstdio)
endif()
