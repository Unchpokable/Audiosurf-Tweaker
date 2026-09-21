# Phase 0 load probe (Docs/Internal/plugin-offline-mode.md, "Ф0") - a measuring DLL for engine\channels\,
# not part of TweakerPlugin. Not in the default ALL build; request explicitly:
#   cmake --build --preset loadprobe
# Output: build/x86-release/bin/TweakerLoadProbe.dll

add_library(load_probe SHARED EXCLUDE_FROM_ALL
    load_probe.cxx
)

# Detours only. No dxsdk, no d3d9/dinput8 import libraries: an import would map those modules the moment
# the probe loads, which is exactly the timing the probe exists to observe.
target_link_libraries(load_probe PRIVATE
    detours
)

set_target_properties(load_probe PROPERTIES
    OUTPUT_NAME "TweakerLoadProbe"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
