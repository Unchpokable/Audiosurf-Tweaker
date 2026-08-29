# Skybox Replacer. DLL-only by nature - every one of these files talks to a live D3D9 device or to
# the game's own Quest3D channels, so unlike src/ui there is nothing here smoke_test could exercise.
target_sources(TweakerPlugin PRIVATE
    skybox.cxx
    sky_bytecode.cxx
    sky_caps.cxx
    sky_catalog.cxx
    sky_compile.cxx
    sky_params.cxx
    sky_target.cxx
    sky_timer.cxx
    sky_paths.cxx
    sky_program.cxx
    sky_shader.cxx
    sky_ui.cxx
    sky_panel.cxx
    skybox_config.cxx
    sky_cubemap.cxx
    sky_renderer.cxx
)

# sky_cubemap decodes the packed cross images with stb_image and reads them through tw::resource.
#
# Nothing here links a D3DX9 import library, and skybox/sky_math.hxx exists precisely so it never
# has to - see the note at the top of that header, and cmake/DirectXSDK.cmake.
target_link_libraries(TweakerPlugin PRIVATE libstb tweaker_resource)

# sky_ui and sky_panel are the adapters that draw the overlay tab and its parameter window; they are
# the only files here that touch tweaker_ui.
target_link_libraries(TweakerPlugin PRIVATE tweaker_ui)
