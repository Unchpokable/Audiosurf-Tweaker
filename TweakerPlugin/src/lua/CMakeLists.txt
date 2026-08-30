# LuaJIT scripting layer. DLL-only: lua_channels reaches into the game's Quest3D graph and lua_api
# draws through the overlay's ImGui context, neither of which smoke_test has.
target_sources(TweakerPlugin PRIVATE
    lua_channels.cxx
    lua_api.cxx
    lua_host.cxx
    lua_config.cxx
    lua_ui.cxx
)

