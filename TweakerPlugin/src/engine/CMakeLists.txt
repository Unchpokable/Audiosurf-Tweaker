# The Quest3D engine as an object: the frame spine, the symbols it needs, the frame clock, the group
# registry and lifecycle, and - under family/ - one file per channel family, each owning the meaning
# of its own vtable slots.
#
# DLL-only, like lua/: every file here reaches into HighPoly.dll, which smoke_test does not have.
# The channel half used to be src/lua/lua_channels.cxx; it was never Lua-specific (plugin/music.cxx
# reads the graph too) and moved here in Ф3 of Docs/Internal/lua-engine-fix-roadmap.md.
target_sources(TweakerPlugin PRIVATE
    engine_symbols.cxx
    engine_control.cxx
    engine_groups.cxx
    engine_state.cxx
    engine_frame.cxx
    channel_vtable.cxx
    channel_kind.cxx
    channel_ref.cxx
    family/fam_number.cxx
    family/fam_text.cxx
    family/fam_vector.cxx
    family/fam_matrix.cxx
    family/fam_table.cxx
)
