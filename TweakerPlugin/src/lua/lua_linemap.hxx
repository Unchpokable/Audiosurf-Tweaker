#pragma once

// The .map file twllink writes beside a bundled script, and what the layer does with it
// (lua-engine-fix-roadmap.md §9.6).
//
// A bundle is many source files glued into one, so a runtime error says "puzzlepro_hud.lua:425" where
// the author needs "scan.lua:88". The map turns the first into the second. It is optional and changes
// nothing the VM does - LuaJIT never learns there were sources - so every failure here degrades to
// "positions are shown as bundle lines", never to a script that does not load.
//
//     # twllink 1
//     # bundle fnv1a64 9f2c...     hash of the bundle it was written for
//     1       -           0        from bundle line 1 on: the bundle's own lines (header, shim)
//     32      scan.lua    1        from bundle line 32 on: scan.lua, starting at its line 1
//     90      scan.lua    60       a stripped comment took lines out: a new run starts here
//
// Each entry starts a run of lines that follow on one-for-one; a position is looked up as the last
// entry at or before it, plus the offset into it. The file "-" is the bundle itself: those positions
// stay as they are.
//
// The hash is what keeps a map from lying. A player who edits the installed .lua by hand leaves the
// old .map beside it, and every position after the edit would then be quietly wrong - worse than no
// map at all. Carriage returns are not hashed: git on Windows may hand the same bundle out with CRLF.
namespace tw::lua
{
inline constexpr std::uint32_t k_own_lines = 0xFFFF'FFFFu;

struct line_run {
    std::uint32_t bundle_line = 0;
    std::uint32_t source_line = 0;
    std::uint32_t file = k_own_lines; // index into line_map::files
};

struct line_map {
    std::vector<std::string> files;
    std::vector<line_run> runs; // strictly increasing bundle_line

    [[nodiscard]] bool empty() const noexcept
    {
        return runs.empty();
    }
};

enum class line_map_status {
    absent,    // no .map - a script written by hand, the normal case
    loaded,    //
    malformed, // `detail` says where
    stale,     // written for a different bundle than the one on disk
};

// FNV-1a 64 over the bytes, carriage returns skipped. twllink computes the same thing.
[[nodiscard]] std::uint64_t bundle_hash(std::string_view bytes) noexcept;

// Parses the text of a .map. On failure `out` is left empty and `detail` names the offending line.
[[nodiscard]] bool parse_line_map(std::string_view text, line_map& out, std::uint64_t& hash, std::string& detail) noexcept;

// `<script>.map` beside the script, checked against the script's current bytes. Cold path: called
// once per run of a script, never per frame.
[[nodiscard]] line_map_status load_line_map(const std::filesystem::path& script, line_map& out, std::string& detail) noexcept;

// Every "<chunk>:<line>" in `text` - Lua's position prefix, a traceback frame, a tw.warn place - with
// the line inside a module rewritten to "<module file>:<line>". `chunk` is what Lua prints for the
// script, its file name; a name too long for Lua's position buffer comes out as "...<tail>" and is
// matched too. Returns false, leaving `out` alone, when nothing was rewritten.
[[nodiscard]] bool map_positions(const line_map& map, std::string_view chunk, std::string_view text, std::string& out);
} // namespace tw::lua
