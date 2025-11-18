# Source files
set(MMAP_SOURCES
    mmap32.cxx
    main.cxx
    proc_check.cxx
    proc_tools.cxx
    win_handle.cxx
)

set(MMAP_HEADERS
    global.h
    context32.h
    mmap32.hxx
    proc_check.hxx
    proc_tools.hxx
    win_handle.hxx
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
        /W4 # Warning level 4
        /WX- # Warnings are not errors
        /permissive- # Conformance mode
        /EHs-c- # Disable exceptions
        /GR- # Disable RTTI
    )
    target_compile_definitions(mmap PRIVATE
        _HAS_EXCEPTIONS=0 # Disable STL exceptions
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(mmap PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -fno-exceptions # Disable exceptions
        -fno-rtti # Disable RTTI
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(mmap PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -fno-exceptions # Disable exceptions
        -fno-rtti # Disable RTTI
    )
else()
    # Fallback for other compilers
    target_compile_options(mmap PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
    message(WARNING "Unknown compiler: exceptions and RTTI may not be disabled")
endif()

# Set executable properties
set_target_properties(mmap PROPERTIES
    OUTPUT_NAME "mmap_${MMAP_ARCHITECTURE}"
    WIN32_EXECUTABLE FALSE
)
