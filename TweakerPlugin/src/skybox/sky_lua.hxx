#pragma once

#include "skybox/sky_image.hxx"
#include "skybox/sky_package.hxx"
#include "skybox/sky_shared.hxx"

// The sky's own Lua state: where a `.sky` package's generator scripts run.
//
// Deliberately **not** tw::lua::host. That VM exists to sit on the engine's data path - it holds
// channel subscriptions, draws through the overlay's ImGui context and dispatches every frame. A
// generator script needs none of that and must not be able to reach any of it: it produces a list of
// sprites and nothing else. Two states, two sandboxes, and this one can be cut far harder.
//
// # What a generator script is
//
// A file inside the package, named by its layer:
//
//   { "id": "clouds", "kind": "sprites", "generator": "scripts/clouds.lua" }
//
// It declares one function, which the engine calls when the layer is (re)built:
//
//   function place()
//       for i = 1, tw.prop("count") do
//           tw.emit_at(azimuth_deg, elevation_deg, roll_deg, half_w_deg, half_h_deg, tile)
//       end
//   end
//
// It emits **sprites, not vertices**. The billboard basis, the pole handling, the atlas rectangles
// and the vertex layout are engine invariants rather than authorial choices, and exposing the buffer
// would both freeze the vertex format as public API and make every script re-derive the hairy ball
// theorem. What is authorial is the distribution, and that is exactly what this hands over.
//
// # Why lua_CFunction here, when lua-scripting.md §2.2 argues for FFI
//
// That argument is about the *hot path*: a lua_CFunction is not JIT-compiled, which matters when a
// script runs per frame or per channel call. `place()` runs once per rebuild - when a knob moves or
// a sky is chosen - and emits at most a few thousand sprites. At roughly a hundred nanoseconds per
// call that is a fraction of a millisecond, against which an FFI bootstrap, a cdef and a pointer
// table would be complexity bought for nothing measurable. Different path, different answer.
namespace tw::skybox::lua
{
// One sprite, as a script emits it. The engine turns each into four vertices with a tangent basis.
//
// `roll` rather than an orientation vector: on a skybox the quad must face the origin - the camera
// does not translate relative to the sky - so where it points is geometry rather than a choice, and
// the only real degree of freedom is the turn about its own view axis. Three numbers for one would
// invite a state the engine then has to either reject or silently correct.
struct sprite {
    std::array<float, 3> dir {}; // normalised by the engine
    float roll {};               // radians
    std::array<float, 2> size {}; // angular half-size, radians; two because clouds are not discs
    int tile {};
};

// What the script may read while it runs.
struct context {
    const package::file_system* files {};
    const shared::state* values {};

    // The layer's own knobs, so `tw.prop("count")` answers with what the manifest declared and the
    // user has since moved. A script asking for a prop the manifest does not declare is an error,
    // not a nil - see the note on run_place.
    std::span<const sky_param> params;

    unsigned int seed {};

    // Refused past this many sprites. The engine sizes the vertex buffer from what comes back, so
    // this is the difference between a typo in a loop bound and an allocation nobody asked for.
    int max_sprites { 8192 };

    // Wall-clock budget for one call. A script with an accidental infinite loop would otherwise hang
    // the render thread; the count hook checks against this and aborts.
    std::chrono::milliseconds budget { 2000 };
};

struct place_result {
    std::vector<sprite> sprites;

    // Empty on success. Otherwise what went wrong, already formatted for the Skybox tab - a compile
    // error with a line number, a runtime error with a traceback, or the budget being spent.
    std::string error;

    [[nodiscard]] bool ok() const noexcept
    {
        return error.empty();
    }
};

// What a `fill()` call is given and what it produces.
//
// The script composes float RGBA layers - see sky_image - and hands one back per tile. It never
// receives a pointer to any of them: a layer is an integer handle into a table the engine owns, so
// the script cannot outlive one, free one twice, or write past the end of one. LuaJIT does not bounds
// check cdata, so handing over a raw buffer would make the ffi sandbox beside the point.
struct fill_context {
    const package::file_system* files {};

    std::span<const sky_param> params;

    int tiles { 4 };
    int size { 256 };

    unsigned int seed {};

    // Total float RGBA texels a script may hold at once, across every layer. A composite naturally
    // wants a handful of scratch layers; a loop that forgets to stop wants all the memory there is.
    std::size_t max_texels { 64u * 1024u * 1024u };

    std::chrono::milliseconds budget { 5000 };
};

struct fill_result {
    // One per tile, in tile order, each `size` square. A tile the script did not hand back comes out
    // empty, which the caller must treat as a failure rather than draw as a hole.
    std::vector<image::layer> tiles;

    std::string error;

    [[nodiscard]] bool ok() const noexcept
    {
        return error.empty();
    }
};

// Loads (or reuses) `script` from the package and calls its `fill()`.
//
// Same loader and same VM as run_place, so a package may keep both functions in one file - which is
// the normal case, since a sky's clouds are one idea.
[[nodiscard]] fill_result run_fill(std::string_view script, const fill_context& ctx);

// Loads (or reuses) `script` from the package and calls its `place()`.
//
// The script is compiled once and kept, keyed on its path and write time, so a rebuild triggered by
// a slider costs a call rather than a compile - and saving the file recompiles it, which is the same
// hot-reload loop the shaders already have.
//
// Never throws and never partially succeeds: on any error the result carries the message and no
// sprites, and the caller should keep whatever it had rather than draw an empty layer.
[[nodiscard]] place_result run_place(std::string_view script, const context& ctx);

// Drops the cached state. From the sky being switched or the package reloaded - a new sky gets a new
// VM rather than inheriting whatever globals the last script left behind.
void reset() noexcept;
} // namespace tw::skybox::lua
