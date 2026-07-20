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
# it. It is not vendored in this repository. Point DXSDK_DIR at a local copy,
# either via -DDXSDK_DIR=... or the DXSDK_DIR environment variable (the same
# variable the original installer used to set). Defaults to the conventional
# in-tree location. A copy of the SDK can be obtained from the archival
# mirror at https://github.com/testing-laboratory/DirectX-SDK-June2010

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
        "Microsoft discontinued this SDK; it is not distributed with this repository.\n"
        "Grab a copy from the archival mirror and point CMake at its root, e.g.:\n"
        "  git clone https://github.com/testing-laboratory/DirectX-SDK-June2010 \"${CMAKE_SOURCE_DIR}/DirectX-SDK-June2010\"\n"
        "or\n"
        "  cmake --preset x86-release -DDXSDK_DIR=\"C:/path/to/DirectX SDK\"\n"
        "or set the DXSDK_DIR environment variable before configuring.")
endif()

add_library(dxsdk INTERFACE)
target_include_directories(dxsdk INTERFACE "${DXSDK_DIR}/Include")
