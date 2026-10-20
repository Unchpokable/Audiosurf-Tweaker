-- ---------------------------------------------------------------------------------------------------
-- tw.abi - every C entry point, bound once, by name.
--
-- This is where the whole FFI story of Docs/Internal/lua-scripting.md §2.2 actually happens: every
-- call a script makes into the plugin goes through one of the ffi.cast'ed pointers below, so it
-- compiles into the trace as a direct call instead of aborting it the way a lua_CFunction would. The
-- prelude then deletes `ffi` from the environment (tw.sandbox, the last section) - the already-cast pointers
-- keep working because they are values, not names, which is exactly the property §6 relies on to keep
-- the fast path without handing scripts arbitrary memory access.
--
-- **By name, not by position.** The table used to be positional - P[1]..P[57] - and an entry
-- inserted in the middle of the C list silently shifted every function after it onto the wrong
-- signature. Now both sides are checked against each other here, at startup, in both directions: a
-- name C offers that has no signature below, or a signature below whose name C does not offer, stops
-- the prelude with that name in the message. Order no longer means anything.
-- ---------------------------------------------------------------------------------------------------

local ffi = require("ffi")
local S = ...

local P = S.ptrs
S.ptrs = nil

-- Signatures must match the declarations in src/lua/api/*.hxx. extern "C" on x86 MSVC is __cdecl,
-- which is also the FFI's default, so no calling-convention decoration is needed.
local SIG = {
    -- api_core
    tw_script_enter       = "int (*)(int, int)",
    tw_script_leave       = "void (*)(int, int, const char*)",
    tw_script_order       = "int (*)(int)",
    tw_script_file        = "const char* (*)(int)",
    tw_diag               = "void (*)(int, int, const char*, const char*)",
    tw_log                = "void (*)(const char*)",
    tw_notify             = "void (*)(int, const char*)",
    tw_engine_ready       = "int (*)(void)",
    tw_can_write          = "int (*)(void)",
    tw_state              = "int (*)(void)",
    tw_state_name         = "const char* (*)(void)",
    tw_ready              = "int (*)(void)",
    tw_graph_revision     = "int (*)(void)",
    tw_group_count        = "int (*)(void)",
    tw_group_name         = "const char* (*)(int)",
    tw_group_loaded       = "int (*)(const char*)",
    tw_frame              = "int (*)(void)",
    tw_dt                 = "float (*)(void)",
    tw_ease               = "float (*)(int, float)",
    tw_ease_count         = "int (*)(void)",
    tw_ease_name          = "const char* (*)(int)",

    -- api_channels
    tw_channel_resolve    = "void* (*)(const char*, const char*, int, int*)",
    tw_channel_resolve_at = "void* (*)(const char*, int, int, int*)",
    tw_ref_free           = "void (*)(void*)",
    tw_kind_name          = "const char* (*)(int)",
    tw_channel_get        = "float (*)(void*)",
    tw_channel_set        = "void (*)(void*, float)",
    tw_channel_text       = "const char* (*)(void*)",
    tw_channel_vector     = "int (*)(void*, float*)",
    tw_channel_set_vector = "void (*)(void*, float, float, float)",
    tw_channel_matrix     = "int (*)(void*, float*)",
    tw_channel_set_matrix = "void (*)(void*, const float*)",
    tw_channel_live       = "int (*)(void*)",
    tw_array_read         = "float (*)(void*, void*, float)",
    tw_array_read_vector  = "int (*)(void*, void*, float, float*)",
    tw_array_write        = "int (*)(void*, void*, float, float)",
    tw_array_write_vector = "int (*)(void*, void*, float, float, float, float)",
    tw_array_rows         = "int (*)(void*)",

    -- api_hooks
    tw_on_call            = "int (*)(int, const char*, const char*, int, int*)",
    tw_on_call_at         = "int (*)(int, const char*, int, int, int*)",
    tw_mute               = "int (*)(int, const char*, const char*, int*)",
    tw_mute_at            = "int (*)(int, const char*, int, int*)",
    tw_mute_set           = "void (*)(int, int)",

    -- api_hud
    tw_theme_count        = "int (*)(void)",
    tw_theme_name         = "const char* (*)(int)",
    tw_theme_color        = "unsigned int (*)(int)",
    tw_font_count         = "int (*)(void)",
    tw_font_name          = "const char* (*)(int)",
    tw_hud_text           = "void (*)(float, float, unsigned int, const char*)",
    tw_hud_text_sized     = "void (*)(float, float, unsigned int, const char*, float)",
    tw_hud_text_font      = "void (*)(float, float, unsigned int, const char*, float, int)",
    tw_hud_text_glow      = "void (*)(float, float, unsigned int, unsigned int, const char*, float, int, float)",
    tw_hud_measure        = "void (*)(const char*, float, float*)",
    tw_hud_measure_font   = "void (*)(const char*, float, int, float*)",
    tw_hud_rect           = "void (*)(float, float, float, float, unsigned int, float, float)",
    tw_hud_rect_corners   = "void (*)(float, float, float, float, unsigned int, float, float, int)",
    tw_hud_rect_glow      = "void (*)(float, float, float, float, unsigned int, float, float)",
    tw_hud_rect_gradient  = "void (*)(float, float, float, float, unsigned int, unsigned int, int)",
    tw_hud_line           = "void (*)(float, float, float, float, unsigned int, float)",
    tw_hud_icon           = "void (*)(const char*, float, float, float, unsigned int)",
    tw_hud_metric         = "float (*)(int)",
    tw_hud_widget_rect    = "int (*)(int, float*)",
    tw_hud_occupied       = "int (*)(float*, int*, int)",
    tw_hud_claim          = "void (*)(float, float, float, float)",
    tw_hud_place          = "int (*)(float, float, int, float, float*)",

    -- api_ui
    tw_ui_tab             = "int (*)(int, const char*)",
    tw_ui_add             = "int (*)(int, int, const char*)",
    tw_ui_range           = "void (*)(int, double, double)",
    tw_ui_options         = "void (*)(int, const char* const*, int)",
    tw_ui_items           = "void (*)(int, const char* const*, const char* const*, int)",
    tw_ui_height          = "void (*)(int, float)",
    tw_ui_get             = "double (*)(int)",
    tw_ui_set             = "void (*)(int, double)",
    tw_ui_text            = "void (*)(int, int, const char*)",
    tw_ui_flags           = "void (*)(int, int, int)",
    tw_ui_tab_visible     = "int (*)(int)",
    tw_ui_next_event      = "int (*)(int*, double*, int*)",

    -- api_storage (the tapes are tw_json_node*, declared by tw.storage)
    tw_storage_read       = "int (*)(int, const char*, const char**, int*)",
    tw_storage_write      = "int (*)(int, const char*, const char*, int)",
    tw_storage_read_json  = "int (*)(int, const char*, const void**, int*)",
    tw_storage_write_json = "int (*)(int, const char*, const void*, int)",
    tw_storage_exists     = "int (*)(int, const char*)",
    tw_storage_remove     = "int (*)(int, const char*)",
    tw_storage_list       = "const char* (*)(int)",
    tw_storage_error      = "const char* (*)(void)",
}

local C = {}
for name, sig in pairs(SIG) do
    local p = P[name]
    if p == nil then
        error("prelude: the host offers no entry point '" .. name .. "'", 0)
    end
    C[name] = ffi.cast(sig, p)
end

for name in pairs(P) do
    if SIG[name] == nil then
        error("prelude: entry point '" .. name .. "' has no signature in the prelude", 0)
    end
end

S.C = C
