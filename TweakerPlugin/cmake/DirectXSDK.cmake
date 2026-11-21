# The Windows 10/11 SDK still ships d3d9.h, but not the D3DX9 utility headers
# (d3dx9.h, d3dx9math.h, ...) that the Quest3D SDK headers need for
# D3DXVECTOR3/D3DXMATRIX. We pull every DirectX 9 header from the June 2010
# DirectX SDK instead of mixing it with the Windows SDK's copies, to avoid
# the well-known header conflicts between the two.
#
# Nothing here is linked: no D3DX9 *function* is ever called by this project
# (D3DXVECTOR3/D3DXMATRIX construction and operators are inlined from
# d3dx9math.inl), so the DirectX SDK's import libraries are intentionally
# unused - only its Include/ directory is needed.
#
# Microsoft discontinued the June 2010 DirectX SDK and no longer distributes
# it. It is a submodule (.gitmodules, testing-laboratory/DirectX-SDK-June2010),
# so a normal --recurse-submodules clone brings the Include/ and Lib/ trees.
# Point DXSDK_DIR at another copy if you keep one elsewhere, either via
# -DDXSDK_DIR=... or the DXSDK_DIR environment variable (the same variable the
# original installer used to set). Defaults to the in-tree submodule.

if(DEFINED ENV{DXSDK_DIR})
    set(_dxsdk_default "$ENV{DXSDK_DIR}")
else()
    set(_dxsdk_default "${CMAKE_SOURCE_DIR}/DirectX-SDK-June2010")
endif()

set(DXSDK_DIR "${_dxsdk_default}" CACHE PATH
    "Path to a local copy of the DirectX SDK, June 2010 (must contain Include/d3dx9.h)")

if(NOT EXISTS "${DXSDK_DIR}/Include/d3dx9.h")
    message(FATAL_ERROR
        "DirectX SDK (June 2010) not found at \"${DXSDK_DIR}\".\n"
        "It is a submodule now: git submodule update --init TweakerPlugin/DirectX-SDK-June2010\n"
        "or point CMake at another copy:\n"
        "  cmake --preset x86-release -DDXSDK_DIR=\"C:/path/to/DirectX SDK\"\n"
        "or set the DXSDK_DIR environment variable before configuring.")
endif()

add_library(dxsdk INTERFACE)
target_include_directories(dxsdk INTERFACE "${DXSDK_DIR}/Include")
