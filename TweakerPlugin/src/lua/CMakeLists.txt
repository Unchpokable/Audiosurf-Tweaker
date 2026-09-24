# LuaJIT scripting layer. DLL-only: lua_api reaches into the game's Quest3D graph through src/engine/
# and draws through the overlay's ImGui context, neither of which smoke_test has.
target_sources(TweakerPlugin PRIVATE
    lua_api.cxx
    lua_host.cxx
    lua_config.cxx
    lua_ui.cxx
)

