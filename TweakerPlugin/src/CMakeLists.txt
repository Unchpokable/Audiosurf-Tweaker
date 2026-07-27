target_sources(TweakerPlugin PRIVATE dllmain.cxx)

# libuulog first: pch.hxx includes uulog.hh, so every target that uses the PCH needs its include
# path and link dependency available before those targets are declared.
add_subdirectory(libuulog)

add_subdirectory(framework)
add_subdirectory(plugin)
add_subdirectory(ipc)
add_subdirectory(resource)
add_subdirectory(libstb)
add_subdirectory(ui)

target_link_libraries(TweakerPlugin PRIVATE libstb libuulog)
