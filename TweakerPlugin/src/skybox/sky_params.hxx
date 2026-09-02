#pragma once

#include "skybox/sky_bytecode.hxx"

// Parameters a sky program exposes to the overlay.
//
// Where they come from: an annotation block in the shader's own source, delimited by `@sky` inside
// an ordinary block comment. One file, no sidecar - a shader and its knobs travel together, and
// nothing has to be kept in sync by hand.
//
//   /* @sky
//   name    = Dying Light
//   version = 2
//   group = Eclipse
//   param = g_eclipse.x | Eclipse size  | 3.65 | 0.5  | 6.0
//   group = Fog
//   param = g_fog.y     | Fog height    | 0.35 | 0.0  | 1.0
//   color = g_fog_tint  | Fog tint      | 1.0,0.78,0.55
//   */
//
// `group` applies to every parameter after it until the next one, and exists because a sky with a
// knob per layer reaches thirty of them - a flat column of thirty sliders is a list you scroll,
// not a panel you use. Groups are optional; a block with none reads exactly as it did before.
//
// `version` is the escape hatch for a shader that was rewritten rather than edited - see
// param_block::version. Also optional, and absent means zero.
//
// Parameters name *variables*, not registers. Register numbers are fxc's to assign and they move
// the moment anything above them is edited; the name is what the author wrote and what they mean.
// The mapping comes from the compiled shader's own constant table (sky_bytecode).
//
// The block is not HLSL and is not parsed as such - it is a delimited region of text, found by its
// marker and read line by line. Nothing here can be confused by shader syntax, because nothing here
// looks at shader syntax.
namespace tw::skybox
{
struct sky_param {
    std::string label;

    // Heading the overlay files this parameter under. Empty for a parameter declared before any
    // `group` line, which is drawn first and without a heading.
    std::string group;

    // Stable identifier for persistence, derived from the register and components rather than the
    // label: "c0x", "c1xyz". A label is for reading and can be reworded; this cannot.
    std::string key;

    // The same thing with ImGui's hidden-label prefix, built once here rather than concatenated per
    // parameter per frame in the panel. Forty knobs made that invisible; a sky with a knob per layer
    // per hemisphere would not.
    std::string widget_id;

    // Where the value goes in the owning program's constant block. Negative means it does not go
    // there at all: the knob is backed by something else, and exactly one of the two fields below
    // says what.
    int reg { -1 };
    int component {}; // first component written, 0-3 for xyzw
    int count { 1 };  // 1 = a scalar slider, 3 = a colour

    // Which stage's register file `reg` indexes. The two are separate hardware, so a name resolves
    // in one or the other and the value has to be uploaded through a different call.
    //
    // A `fullsky` layer never needs this: it borrows the shared cube vertex shader, which takes only
    // a matrix. A geometry layer does - how its sprites move is vertex work, and a knob for it that
    // could not be named in the manifest would have to be a fixed register nobody can see.
    bool vertex_stage {};

    // A knob whose value belongs to the sky rather than to one layer's registers: the canonical
    // shared path, "lights.primary.bearing". Its value lives in shared::state, and moving it
    // re-applies every binding in every layer that reads it - which is the whole point.
    std::string shared_key;

    // A knob that configures the layer *kind* rather than its shader: "count", "size". A sprite
    // count rebuilds a vertex buffer, and no shader constant can express that.
    std::string property;

    float min_value { 0.f };
    float max_value { 1.f };

    std::array<float, 3> value {};

    // What the annotation asked for, kept so the overlay can put a knob back. The saved value from
    // the settings file overwrites `value` and deliberately not this.
    std::array<float, 3> default_value {};

    // Where this parameter's value is stored, for a program that came from a `.sky` package: the
    // layer it belongs to, and its id within that layer. Both empty for a program loaded from a
    // lone .hlsl, which still keys its settings by register in the flat config.
    //
    // Carried on the parameter rather than kept in a parallel table because the panel edits these
    // one at a time and has to know where to write without knowing which kind of sky it is looking
    // at. One type, two origins - the alternative is the second parameter system this whole format
    // exists to remove.
    std::string settings_layer;
    std::string settings_id;

    [[nodiscard]] bool is_color() const noexcept
    {
        return count == 3;
    }

    [[nodiscard]] bool from_package() const noexcept
    {
        return !settings_layer.empty();
    }

    // Exactly one of these three is true. What backs the knob decides where a move is written and
    // what has to be refreshed afterwards, and it is the only thing the edit path branches on.
    [[nodiscard]] bool is_constant() const noexcept
    {
        return reg >= 0;
    }

    [[nodiscard]] bool is_shared() const noexcept
    {
        return !shared_key.empty();
    }

    [[nodiscard]] bool is_property() const noexcept
    {
        return !property.empty();
    }
};

// A `bind` entry of a `.sky` layer, resolved against that layer's compiled shader: where the value
// lands, and which shared value it comes from.
//
// Resolved once, when the layer is built, rather than every time a knob moves. The name-to-register
// lookup is the expensive half and it only changes when the shader is recompiled - at which point
// the whole list is rebuilt anyway.
struct resolved_binding {
    // Which stage's register file `reg` indexes - see sky_param::vertex_stage.
    bool vertex_stage {};

    int reg { -1 };
    int component {};

    // How many components the target names. Zero means the swizzle was omitted, and the source's own
    // width decides - "g_light" bound to a direction takes three, bound to an intensity takes one.
    int count {};

    // The shared path, evaluated on each apply. Kept as text because that is what it is: the sky's
    // shared block is addressed by name, and pre-resolving it to a pointer would break the moment a
    // manifest reload rebuilt the list it points into.
    std::string source;

    [[nodiscard]] bool valid() const noexcept
    {
        return reg >= 0;
    }
};

struct param_block {
    // Overrides the name the overlay shows, when the block sets one. Empty otherwise, and the file
    // stem is used.
    std::string display_name;

    // Layout generation, from a `version = N` line. Folded into the settings key, so bumping it
    // gives the shader a clean namespace.
    //
    // Exists because saved values are keyed by register, which is right for a rename and wrong for
    // a rewrite: move a knob to a different meaning at the same register and the old value comes
    // back attached to the new control. There is no way to detect that from the outside - c6.x is
    // c6.x - so it is the author who says "this layout is not the previous one".
    int version {};

    std::vector<sky_param> params;
};

// Resolves a reference like "g_eclipse.x" or "g_fog_tint" against a compiled shader's constant
// table, filling in `reg`, `component`, `count` and `key`. False when the shader declares no such
// variable - which fxc will have done for anything the shader does not read.
//
// Public because the `.sky` package loader needs exactly this and nothing else from this module: a
// manifest names variables for the same reason an annotation does, and both have to end up at the
// register the compiler happened to choose.
[[nodiscard]] bool resolve_variable(const bytecode::reflection& reflection, std::string_view reference, int count, sky_param& out);

// Resolves a binding's target - "g_light.xyz", "g_light2.w", "g_tint" - against the same table.
//
// Unlike a parameter, the width comes from the swizzle rather than from the declaration: a binding
// says where a value lands and how much of it lands there. An omitted swizzle leaves `count` at
// zero, meaning "as wide as whatever is bound to it".
//
// False when the shader declares no such variable, or when the swizzle is not a run of consecutive
// components - `.xz` names two places and a binding writes one span.
[[nodiscard]] bool resolve_binding_target(const bytecode::reflection& reflection, std::string_view reference, resolved_binding& out);

// Reads the annotation block out of `source` and resolves its variable names against `reflection`.
//
// Anything that does not resolve is dropped with a warning rather than failing the shader: a
// parameter naming a variable the compiler optimised away is a mistake in the annotation, not a
// reason to refuse to draw the sky.
[[nodiscard]] param_block parse_params(std::string_view source, const bytecode::reflection& reflection);

// The same, for a shared header rather than a program.
//
// The difference is what an unresolved name means. In a program's own block it is a mistake worth
// warning about; in a header it is the normal case - sky_common.hlsli offers knobs for the whole
// palette, and a program that reads three of those six variables should get three knobs and no
// complaints about the other three. fxc drops what a shader does not use, so "does this program
// want this parameter" is answered for free by whether the name is in its constant table.
[[nodiscard]] param_block parse_shared_params(std::string_view source, const bytecode::reflection& reflection);

// Writes every parameter's current value into whichever of the two constant blocks it names.
//
// `vertex_constants` may be empty, which is the ordinary case: every `fullsky` sky borrows the
// shared cube vertex shader and has nothing to configure in it.
void apply_params(std::span<const sky_param> params, std::span<float> constants, std::span<float> vertex_constants) noexcept;

// Brings parameters of the same group together, groups in order of first appearance and members in
// their existing order.
//
// Needed because the list the overlay draws is two blocks merged - the shared header's, then the
// program's - and a program that re-declares a shared variable replaces it *in the shared block's
// position*. Without this, that parameter's heading would appear twice: once where it was, once
// where the rest of its group is.
void order_by_group(std::vector<sky_param>& params);
} // namespace tw::skybox
