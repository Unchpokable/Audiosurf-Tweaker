# LunaSVG (+ its plutovg submodule) - SVG rasterizer behind tw::ui::image::svg. Unlike the other
# dependencies in this directory, this one is neither vendored nor proprietary, so it is fetched
# rather than expected on disk.

include(FetchContent)

# FetchContent trees land outside every build/ directory, so they survive the `cmake --fresh` the
# TweakerUI.csproj pre-build hook runs on every dotnet build (that wipes CMakeCache.txt and
# CMakeFiles/, but not this). Without it, every managed build would re-clone lunasvg.
#
# Bucketed by generator, and that part is not optional: FetchContent drives the download through a
# nested project in <base>/<name>-subbuild, whose CMakeCache.txt is bound to the generator that
# created it. This project is configured by two different ones - Ninja from the developer presets,
# Visual Studio from the pre-build hook (it forces -A Win32 to get x86 output regardless of the
# calling shell) - and pointing both at one base dir makes the second fail outright with
# "generator ... does not match the generator used previously". One bucket each costs a second
# clone and nothing else; both Ninja presets still share theirs.
string(REGEX REPLACE "[^A-Za-z0-9]+" "-" LUNASVG_GENERATOR_SLUG "${CMAKE_GENERATOR}")
string(TOLOWER "${LUNASVG_GENERATOR_SLUG}" LUNASVG_GENERATOR_SLUG)
set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/.deps/${LUNASVG_GENERATOR_SLUG}" CACHE PATH "" FORCE)

# ON by default upstream; the examples pull in extra targets we never build.
set(LUNASVG_BUILD_EXAMPLES OFF)
# The overlay only ever renders path-based icons. Enumerating the system font collection from
# inside a DLL injected into the game buys nothing and touches the registry/font directories on
# first use.
set(LUNASVG_DISABLE_LOAD_SYSTEM_FONTS ON)
# plutovg comes from lunasvg's own git submodule (its CMakeLists does add_subdirectory(plutovg)).
# FetchContent initialises submodules recursively by default, so no second declaration is needed.
set(USE_SYSTEM_PLUTOVG OFF)

FetchContent_Declare(lunasvg
    GIT_REPOSITORY https://github.com/sammycage/lunasvg.git
    GIT_TAG v3.5.0
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(lunasvg)

# BUILD_SHARED_LIBS is never set in this project, so lunasvg builds STATIC and defines
# LUNASVG_BUILD_STATIC on its own PUBLIC interface - consumers just link the target.
if(MSVC)
    foreach(LUNASVG_TARGET lunasvg plutovg)
        if(NOT TARGET ${LUNASVG_TARGET})
            continue()
        endif()

        # These targets inherited the project-wide /std:c++latest: the root add_compile_options()
        # seeds the directory COMPILE_OPTIONS property, which CMake copies into the COMPILE_OPTIONS
        # of every target created under it. lunasvg also sets CXX_STANDARD 17, and that lands on the
        # command line FIRST - so /std:c++latest wins, cl warns D9025 on every translation unit, and
        # upstream silently compiles at a standard it never asked for. Strip it back out so it
        # builds as the C++17 it declares.
        #
        # Has to be done per target, not by clearing the directory property: that property is only
        # an initializer, read when a target is created, so changing it after
        # FetchContent_MakeAvailable() has no effect at all.
        get_target_property(LUNASVG_OPTS ${LUNASVG_TARGET} COMPILE_OPTIONS)
        if(LUNASVG_OPTS)
            list(FILTER LUNASVG_OPTS EXCLUDE REGEX "std:c\\+\\+latest")
            set_target_properties(${LUNASVG_TARGET} PROPERTIES COMPILE_OPTIONS "${LUNASVG_OPTS}")
        endif()

        # Third-party, same treatment as imgui / libstb: keep it out of our warning level rather
        # than patching vendor sources.
        target_compile_options(${LUNASVG_TARGET} PRIVATE /W0)
    endforeach()
endif()
