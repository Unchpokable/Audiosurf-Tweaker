# LuaJIT 2.1 (rolling), built from the LuaJIT/ git submodule.
#
# Upstream ships only src/msvcbuild.bat for MSVC, so this mirrors that script as CMake targets -
# the same approach, and for the same reason, as cmake/Detours.cmake mirroring its nmake Makefile:
# it then works identically under the developer Ninja presets and under the Visual Studio generator
# the TweakerUI.csproj pre-build hook forces, with no dependency on which shell invoked the build.
#
# LuaJIT cannot be compiled straight from its sources: an interpreter core written in DynASM has to
# be assembled first by tools that are themselves built from the same tree. The pipeline is
#
#   minilua                     -> a stripped Lua 5.1, used only to run DynASM
#   minilua dynasm.lua vm_x86   -> host/buildvm_arch.h   (the assembled VM, as C data)
#   minilua genversion.lua      -> luajit.h              (rolling version stamped in)
#   buildvm                     -> lj_vm.obj + 6 generated headers + jit/vmdef.lua
#   cl lj_*.c lib_*.c           -> the library proper, which #includes those headers
#
# Everything generated lands in ${CMAKE_BINARY_DIR}/luajit-gen so the submodule working tree stays
# clean (msvcbuild.bat writes into src/ and then deletes some of it again).

set(LUAJIT_DIR "${CMAKE_SOURCE_DIR}/LuaJIT")
set(LUAJIT_SRC "${LUAJIT_DIR}/src")
set(LUAJIT_GEN "${CMAKE_BINARY_DIR}/luajit-gen")

if(NOT EXISTS "${LUAJIT_SRC}/lj_obj.h")
    message(FATAL_ERROR
        "LuaJIT submodule not found at \"${LUAJIT_DIR}\".\n"
        "Fetch it with:\n"
        "  git submodule update --init --recursive")
endif()

file(MAKE_DIRECTORY "${LUAJIT_GEN}")
file(MAKE_DIRECTORY "${LUAJIT_GEN}/jit")

# ---------------------------------------------------------------- host tools
#
# Built for x86 like everything else here. That is not a compromise: this project is x86-only (see
# the guard at the top of the root CMakeLists.txt), and a 32-bit host tool runs fine under WOW64 on
# the x64 machines that actually do the building. It also sidesteps msvcbuild.bat's trick of running
# minilua bare and reading its exit code to discover the pointer size - we already know it is 4.

add_executable(minilua "${LUAJIT_SRC}/host/minilua.c")
target_compile_definitions(minilua PRIVATE _CRT_SECURE_NO_DEPRECATE)
set_target_properties(minilua PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${LUAJIT_GEN}")
if(MSVC)
    target_compile_options(minilua PRIVATE /W0)
endif()

# DynASM flags for Windows/x86, copied verbatim from msvcbuild.bat's :NO32 branch. FPU is present,
# P64 (64-bit pointers) is deliberately absent.
set(LUAJIT_DASM_FLAGS -D WIN -D JIT -D FFI -D ENDIAN_LE -D FPU)

add_custom_command(
    OUTPUT "${LUAJIT_GEN}/buildvm_arch.h"
    COMMAND minilua "${LUAJIT_DIR}/dynasm/dynasm.lua" -LN ${LUAJIT_DASM_FLAGS}
            -o "${LUAJIT_GEN}/buildvm_arch.h" "${LUAJIT_SRC}/vm_x86.dasc"
    DEPENDS minilua "${LUAJIT_SRC}/vm_x86.dasc"
    WORKING_DIRECTORY "${LUAJIT_SRC}"
    COMMENT "LuaJIT: assembling the x86 VM core (DynASM)"
    VERBATIM
)

# The rolling release version is the commit timestamp. Upstream reads it from git during the build;
# .relver in the tree holds an unexpanded $Format:%ct$ placeholder unless the tree came from a
# release tarball. Resolved once at configure time so the build itself never shells out to git.
find_package(Git QUIET)
set(LUAJIT_RELVER "")
if(GIT_FOUND)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" show -s --format=%ct
        WORKING_DIRECTORY "${LUAJIT_DIR}"
        OUTPUT_VARIABLE LUAJIT_RELVER
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()
if(NOT LUAJIT_RELVER MATCHES "^[0-9]+$")
    # genversion.lua falls back to the literal string "ROLLING" in the version macros, which is
    # exactly what upstream does when git is unavailable. Nothing depends on the number.
    set(LUAJIT_RELVER "ROLLING")
    message(STATUS "LuaJIT: rolling version unresolved (no git metadata) - stamping ROLLING")
endif()
file(WRITE "${LUAJIT_GEN}/luajit_relver.txt" "${LUAJIT_RELVER}\n")

add_custom_command(
    OUTPUT "${LUAJIT_GEN}/luajit.h"
    COMMAND minilua "${LUAJIT_SRC}/host/genversion.lua"
            "${LUAJIT_SRC}/luajit_rolling.h" "${LUAJIT_GEN}/luajit_relver.txt" "${LUAJIT_GEN}/luajit.h"
    DEPENDS minilua "${LUAJIT_SRC}/luajit_rolling.h" "${LUAJIT_GEN}/luajit_relver.txt"
    COMMENT "LuaJIT: stamping version header"
    VERBATIM
)

file(GLOB LUAJIT_BUILDVM_SRC "${LUAJIT_SRC}/host/buildvm*.c")
add_executable(buildvm ${LUAJIT_BUILDVM_SRC} "${LUAJIT_GEN}/buildvm_arch.h" "${LUAJIT_GEN}/luajit.h")
target_include_directories(buildvm PRIVATE "${LUAJIT_GEN}" "${LUAJIT_SRC}" "${LUAJIT_DIR}/dynasm")
target_compile_definitions(buildvm PRIVATE _CRT_SECURE_NO_DEPRECATE)
set_target_properties(buildvm PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${LUAJIT_GEN}")
if(MSVC)
    target_compile_options(buildvm PRIVATE /W0)
endif()

# ------------------------------------------------------- buildvm's generated output
#
# ALL_LIB from msvcbuild.bat: the order matters, it fixes the library ids baked into the bytecode.
set(LUAJIT_ALL_LIB
    lib_base.c lib_math.c lib_bit.c lib_string.c lib_table.c lib_io.c
    lib_os.c lib_package.c lib_debug.c lib_jit.c lib_ffi.c lib_buffer.c
)
set(LUAJIT_ALL_LIB_PATHS "")
foreach(LUAJIT_LIB_FILE ${LUAJIT_ALL_LIB})
    list(APPEND LUAJIT_ALL_LIB_PATHS "${LUAJIT_SRC}/${LUAJIT_LIB_FILE}")
endforeach()

# lj_vm.obj is the assembled interpreter. It is emitted as a COFF object rather than C, and is
# linked into the library as-is (see EXTERNAL_OBJECT below).
add_custom_command(
    OUTPUT "${LUAJIT_GEN}/lj_vm.obj"
    COMMAND buildvm -m peobj -o "${LUAJIT_GEN}/lj_vm.obj"
    DEPENDS buildvm
    COMMENT "LuaJIT: emitting interpreter object (lj_vm.obj)"
    VERBATIM
)

set(LUAJIT_GENERATED_HEADERS "")
foreach(LUAJIT_MODE bcdef ffdef libdef recdef)
    add_custom_command(
        OUTPUT "${LUAJIT_GEN}/lj_${LUAJIT_MODE}.h"
        COMMAND buildvm -m ${LUAJIT_MODE} -o "${LUAJIT_GEN}/lj_${LUAJIT_MODE}.h" ${LUAJIT_ALL_LIB_PATHS}
        DEPENDS buildvm ${LUAJIT_ALL_LIB_PATHS}
        COMMENT "LuaJIT: generating lj_${LUAJIT_MODE}.h"
        VERBATIM
    )
    list(APPEND LUAJIT_GENERATED_HEADERS "${LUAJIT_GEN}/lj_${LUAJIT_MODE}.h")
endforeach()

add_custom_command(
    OUTPUT "${LUAJIT_GEN}/lj_folddef.h"
    COMMAND buildvm -m folddef -o "${LUAJIT_GEN}/lj_folddef.h" "${LUAJIT_SRC}/lj_opt_fold.c"
    DEPENDS buildvm "${LUAJIT_SRC}/lj_opt_fold.c"
    COMMENT "LuaJIT: generating lj_folddef.h"
    VERBATIM
)
list(APPEND LUAJIT_GENERATED_HEADERS "${LUAJIT_GEN}/lj_folddef.h")

# vmdef.lua is only consumed by the jit.* debug modules (jit/dump.lua and friends), which this
# plugin never loads - generated anyway so the tree matches an upstream build, and it costs nothing.
add_custom_command(
    OUTPUT "${LUAJIT_GEN}/jit/vmdef.lua"
    COMMAND buildvm -m vmdef -o "${LUAJIT_GEN}/jit/vmdef.lua" ${LUAJIT_ALL_LIB_PATHS}
    DEPENDS buildvm ${LUAJIT_ALL_LIB_PATHS}
    COMMENT "LuaJIT: generating jit/vmdef.lua"
    VERBATIM
)
list(APPEND LUAJIT_GENERATED_HEADERS "${LUAJIT_GEN}/jit/vmdef.lua")

# ---------------------------------------------------------------- the library
#
# Globbed rather than listed, mirroring msvcbuild.bat's own `lj_*.c lib_*.c`. Safe here in a way it
# usually is not: the submodule is pinned, so the file set only ever changes when someone
# deliberately moves the pin and re-runs CMake anyway. ljamalg.c and luajit.c do not match either
# pattern, which is what keeps the amalgamated build and the CLI's main() out of this.
file(GLOB LUAJIT_CORE_SRC "${LUAJIT_SRC}/lj_*.c" "${LUAJIT_SRC}/lib_*.c")

add_library(luajit STATIC ${LUAJIT_CORE_SRC} ${LUAJIT_GENERATED_HEADERS} "${LUAJIT_GEN}/lj_vm.obj")
set_source_files_properties("${LUAJIT_GEN}/lj_vm.obj" PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)

# Include paths split out into their own interface target: pch.hxx pulls in lua.hpp, and every
# target compiling that PCH therefore needs these paths - including tweaker_resource, which has no
# business linking the VM itself (same reasoning as its dxsdk/detours/quest3d_sdk entries).
#
# LUAJIT_GEN comes first because lj_arch.h and friends #include "luajit.h", and the only copy of that
# header is the generated one.
add_library(luajit_headers INTERFACE)
target_include_directories(luajit_headers INTERFACE "${LUAJIT_GEN}" "${LUAJIT_SRC}")

target_link_libraries(luajit PUBLIC luajit_headers)
target_compile_definitions(luajit PRIVATE _CRT_SECURE_NO_DEPRECATE)

if(MSVC)
    # /arch:SSE2 is upstream's own flag for x86 and it is load-bearing here, not incidental: the
    # game creates its D3D9 device without D3DCREATE_FPU_PRESERVE (measured - see
    # Docs/Internal/skybox-procedural.md), which leaves the x87 control word in single precision for
    # the whole process. lua_Number is a double; an x87 build would silently lose mantissa bits.
    # SSE2 arithmetic ignores the x87 control word, so LuaJIT is immune. See
    # Docs/Internal/lua-scripting.md §2.1.
    target_compile_options(luajit PRIVATE /arch:SSE2 /W0)
endif()
