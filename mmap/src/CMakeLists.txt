# Source files
set(MMAP_SOURCES
    mmap32.cxx
    main.cxx
)

set(MMAP_HEADERS
    global.h
    context32.h
    mmap32.hxx
)

# Create executable
add_executable(mmap ${MMAP_SOURCES} ${MMAP_HEADERS})

# Precompiled headers
target_precompile_headers(mmap PRIVATE global.h)

# Include directories
target_include_directories(mmap
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
        $<INSTALL_INTERFACE:include>
)

# Compiler-specific options
if(MSVC)
    target_compile_options(mmap PRIVATE
        /W4
        /WX-
        /permissive-
    )
else()
    target_compile_options(mmap PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# Set executable properties
set_target_properties(mmap PROPERTIES
    OUTPUT_NAME "mmap_${MMAP_ARCHITECTURE}"
    WIN32_EXECUTABLE FALSE
)
