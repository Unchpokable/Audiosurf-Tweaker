target_sources(TweakerPlugin PRIVATE dllmain.cxx)

add_subdirectory(framework)
add_subdirectory(plugin)
add_subdirectory(ipc)
add_subdirectory(resource)
add_subdirectory(libstb)
add_subdirectory(ui)

target_link_libraries(TweakerPlugin PRIVATE libstb)
