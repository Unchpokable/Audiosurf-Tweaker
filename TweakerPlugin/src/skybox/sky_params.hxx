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

    int reg {};
    int component {}; // first component written, 0-3 for xyzw
    int count { 1 };  // 1 = a scalar slider, 3 = a colour

    float min_value { 0.f };
    float max_value { 1.f };

    std::array<float, 3> value {};

    // What the annotation asked for, kept so the overlay can put a knob back. The saved value from
    // the settings file overwrites `value` and deliberately not this.
    std::array<float, 3> default_value {};

    [[nodiscard]] bool is_color() const noexcept
    {
        return count == 3;
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

// Writes every parameter's current value into a program's constant block.
void apply_params(std::span<const sky_param> params, std::span<float> constants) noexcept;

// Brings parameters of the same group together, groups in order of first appearance and members in
// their existing order.
//
// Needed because the list the overlay draws is two blocks merged - the shared header's, then the
// program's - and a program that re-declares a shared variable replaces it *in the shared block's
// position*. Without this, that parameter's heading would appear twice: once where it was, once
// where the rest of its group is.
void order_by_group(std::vector<sky_param>& params);
} // namespace tw::skybox
