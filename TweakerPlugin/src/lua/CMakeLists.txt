# LuaJIT scripting layer. DLL-only: the api/ files reach into the game's Quest3D graph through
# src/engine/ and draw through the overlay's ImGui context, neither of which smoke_test has.
#
# How the pieces fit together is in lua_host.hxx; why they are cut this way is §3.3 of
# Docs/Internal/lua-engine-fix-roadmap.md.
target_sources(TweakerPlugin PRIVATE
    lua_host.cxx
    lua_vm.cxx
    lua_prelude.cxx
    lua_registry.cxx
    lua_script.cxx
    lua_sched.cxx
    lua_diag.cxx
    lua_config.cxx
    lua_ui.cxx
    api/api_core.cxx
    api/api_channels.cxx
    api/api_hooks.cxx
    api/api_hud.cxx
)
