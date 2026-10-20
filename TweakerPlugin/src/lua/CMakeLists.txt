# LuaJIT scripting layer. DLL-only: the api/ files reach into the game's Quest3D graph through
# src/engine/ and draw through the overlay's ImGui context, neither of which smoke_test has.
#
# How the pieces fit together is in lua_host.hxx; why they are cut this way is §3.3 of
# Docs/Internal/lua-engine-fix-roadmap.md.
target_sources(TweakerPlugin PRIVATE
    lua_host.cxx
    lua_hud_space.cxx
    lua_vm.cxx
    lua_prelude.cxx
    lua_registry.cxx
    lua_script.cxx
    lua_sched.cxx
    lua_tabs.cxx
    lua_storage.cxx
    lua_diag.cxx
    lua_log.cxx
    lua_linemap.cxx
    lua_config.cxx
    lua_remote.cxx
    lua_ui.cxx
    api/api_core.cxx
    api/api_channels.cxx
    api/api_hooks.cxx
    api/api_hud.cxx
    api/api_ui.cxx
    api/api_storage.cxx
)

# The prelude - the Lua half of the layer - is prelude/*.lua, compiled into the DLL as text
# (cmake/EmbedText.cmake). lua_prelude.cxx says which file is which section and in what order they run;
# this only makes every file there available to it, under its file stem.
file(GLOB TWEAKER_PRELUDE_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/prelude/*.lua")
list(SORT TWEAKER_PRELUDE_FILES)
list(JOIN TWEAKER_PRELUDE_FILES "|" _tweaker_prelude_inputs)

set(TWEAKER_PRELUDE_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/lua/lua_prelude_text.hxx")
add_custom_command(
    OUTPUT "${TWEAKER_PRELUDE_HEADER}"
    COMMAND "${CMAKE_COMMAND}"
        "-DOUTPUT=${TWEAKER_PRELUDE_HEADER}"
        "-DNAMESPACE=tw::lua::prelude::text"
        "-DINPUTS=${_tweaker_prelude_inputs}"
        -P "${CMAKE_SOURCE_DIR}/cmake/EmbedText.cmake"
    DEPENDS ${TWEAKER_PRELUDE_FILES} "${CMAKE_SOURCE_DIR}/cmake/EmbedText.cmake"
    COMMENT "Embedding the Lua prelude"
    VERBATIM
)
# A target of its own rather than the header in TweakerPlugin's sources: the plugin target is created in the
# root directory, and a custom command's output listed as a source from another directory does not pull the
# command into that target's build - lua_prelude.cxx would compile before the header exists.
add_custom_target(tweaker_prelude_text DEPENDS "${TWEAKER_PRELUDE_HEADER}")
add_dependencies(TweakerPlugin tweaker_prelude_text)
target_include_directories(TweakerPlugin PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
