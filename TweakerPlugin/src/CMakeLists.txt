target_sources(TweakerPlugin PRIVATE dllmain.cxx)

# libuulog first: pch.hxx includes uulog.hh, so every target that uses the PCH needs its include
# path and link dependency available before those targets are declared.
add_subdirectory(libuulog)

add_subdirectory(framework)
add_subdirectory(plugin)

# After plugin: engine_control writes quest3d::g_engine and the lifecycle log.
add_subdirectory(engine)
add_subdirectory(ipc)
add_subdirectory(resource)
add_subdirectory(libstb)
add_subdirectory(libyyjson)
add_subdirectory(libminiz)
add_subdirectory(ui)

# After ui: lua_api pushes notefeed toasts and draws on the overlay's ImGui context.
add_subdirectory(lua)

# After libstb/resource: skybox links both.
add_subdirectory(skybox)

target_link_libraries(TweakerPlugin PRIVATE libstb libuulog)
