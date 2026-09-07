# Vendored miniz (https://github.com/richgel999/miniz), MIT, 3.0.2 - the amalgamated release, two
# files.
#
# Reads the zip form of a `.sky` package. Chosen over the alternatives for the same reason yyjson
# was: it is ANSI C with return codes, so there is no exception model and no foreign C++ error style
# to reconcile with this project's, and it decompresses straight out of a memory buffer - which is
# what sky_vfs hands it, and what lets a package be read without ever writing one of its files to
# disk.
#
# Vendored rather than fetched so a configure needs no network, as libyyjson and libstb are.
add_library(libminiz STATIC
    miniz.c
)

target_include_directories(libminiz PUBLIC
    "${CMAKE_SOURCE_DIR}/src"
)

# PUBLIC, and that is load-bearing rather than tidiness: several of these change the *layout* of the
# structures declared in miniz.h, so a consumer compiled without them would disagree with the library
# about where a field is. Setting them here is what makes that impossible.
#
#   NO_ARCHIVE_WRITING_APIS - nothing here creates a zip. Removing the writer takes the deflate
#                             compressor with it, which is most of the library's size.
#   NO_STDIO                - the archive is read into memory by sky_vfs before miniz sees it. Not
#                             only unnecessary but wanted: a file miniz held open would be a file the
#                             author cannot overwrite while the game has the sky loaded, and
#                             overwriting it is exactly how a package is edited.
#   NO_TIME                 - only the writer stamps entries, and only the extractor restores them.
#   NO_ZLIB_COMPATIBLE_NAMES - keeps `compress`, `uncompress`, `crc32` and friends out of the global
#                             namespace of a DLL that is injected into somebody else's process.
target_compile_definitions(libminiz PUBLIC
    MINIZ_NO_ARCHIVE_WRITING_APIS
    MINIZ_NO_STDIO
    MINIZ_NO_TIME
    MINIZ_NO_ZLIB_COMPATIBLE_NAMES
)

if(MSVC)
    # Third-party; keep the noise down without editing vendor sources.
    target_compile_options(libminiz PRIVATE /W0)
endif()
