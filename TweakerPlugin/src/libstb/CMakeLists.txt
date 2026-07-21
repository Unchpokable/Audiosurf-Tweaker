# Vendored stb (header-only libs compiled once for the whole tree).
add_library(libstb STATIC
    stb_image.cxx
    stb_image_resize2.cxx
)

target_include_directories(libstb PUBLIC
    "${CMAKE_SOURCE_DIR}/src"
)

if(MSVC)
    # stb is third-party; keep noise down without altering vendor headers.
    target_compile_options(libstb PRIVATE /W0)
endif()
