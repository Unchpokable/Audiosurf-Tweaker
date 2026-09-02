#pragma once

#include "skybox/sky_package.hxx"
#include "skybox/sky_params.hxx"

// The sky's shared block at run time: what its lights and shared values currently are, and how a
// layer asks for one.
//
// This is the piece the `.sky` format exists for. Before it, "where is the sun" lived in the sky
// shader's own registers, and anything drawn over that sky could only reach it through an adapter
// that knew both ends - which is how one feature ended up with two parameter systems and four
// consecutive representations of a single direction.
//
// The rule here is one sentence: **a shared value belongs to the sky, not to any layer.** The sky
// owns it, every layer that wants it declares a binding, and no layer knows that any other layer
// exists. That is what makes "the clouds are lit by the sun the sky paints" a fact of the format
// rather than a wire someone remembered to solder.
//
// Generalised past suns deliberately, and this is the important part. Two suns was never the
// interesting number: a sky has a sun, a moon, an aurora, a horizon glow, a planet filling a third
// of the view - and geometry lit by only one of them is geometry that visibly does not belong to
// the sky behind it. Plausible lighting across layers is exactly the property that every layer
// answers to the same list of lights. Nothing below is specific to a star.
//
// # Paths
//
// A binding names its source by path. Everything resolvable:
//
//   lights.<id>.bearing      degrees, as the author typed them (an offset, for a relative light)
//   lights.<id>.elevation    degrees; a light declared `same_as` another resolves to that one's
//   lights.<id>.color        rgb, white when the light declares none
//   lights.<id>.intensity    scalar, 1 when the light declares none
//   lights.<id>.direction    derived: the unit vector, from the *absolute* bearing and elevation
//   lights.<id>.radiance     derived: colour times intensity - what a lighting shader actually wants
//   values.<id>              a loose shared value
//
// `suns.` is accepted for `lights.` and `.strength` for `.intensity`, because the first packages
// were written that way and because for a sky with two suns those are the better words. They are
// spellings, not concepts: everything below knows only the general one.
//
// The authored forms and the derived forms are equally bindable, and that is what removes the
// adapter. A sky shader binds `g_sun.x` to a bearing because degrees are what its author types; a
// cloud shader binds `g_light.xyz` to a direction because a vector is what its maths needs. Neither
// knows how the other prefers to write it down.
namespace tw::skybox::shared
{
// A resolved shared value: one to four floats and how many of them mean anything.
struct value {
    std::array<float, 4> data {};
    int count { 1 };
};

// One knob of the shared block, as the panel draws it. The declaration comes from the manifest and
// the value is what the player has moved it to.
struct knob {
    std::string key; // the canonical path: "lights.primary.bearing", "values.haze"
    std::string label;
    std::string group;

    int count { 1 };

    float min_value { 0.f };
    float max_value { 1.f };

    std::array<float, 3> value {};
    std::array<float, 3> default_value {};

    [[nodiscard]] bool is_color() const noexcept
    {
        return count == 3;
    }
};

// The live shared block of one loaded sky.
//
// Holds the manifest it was built from, because resolving a path needs the structure as well as the
// numbers: a relative bearing is only absolute once you know what it is relative to, and that is a
// fact about the document rather than about any stored value.
class state
{
public:
    // Rebuilds from a manifest. Knobs the new manifest still declares keep whatever value they
    // already had - editing Config.json while the game runs is the development loop this format was
    // built around, and resetting the player's tuning on every save would make it useless.
    void adopt(std::shared_ptr<const package::manifest> sky);

    [[nodiscard]] const package::manifest* manifest() const noexcept
    {
        return m_sky.get();
    }

    [[nodiscard]] std::span<const knob> knobs() const noexcept
    {
        return m_knobs;
    }

    // Moves one knob. False when nothing is stored under that key, which the caller should read as
    // "the manifest no longer declares this" rather than as an error.
    bool write(std::string_view key, std::span<const float> in) noexcept;

    [[nodiscard]] bool read(std::string_view key, std::array<float, 3>& out) const noexcept;

    // Puts every knob back to the default its manifest declared.
    void reset() noexcept;

    // The one entry point a binding uses. False when the path names nothing this sky has - a typo,
    // or a light the author removed - and the caller leaves the target constant alone rather than
    // writing a zero that would look like an authored value.
    [[nodiscard]] bool resolve(std::string_view path, value& out) const noexcept;

private:
    [[nodiscard]] const knob* find(std::string_view key) const noexcept;

    // One authored field of one light, or the neutral answer when the author declared none.
    [[nodiscard]] bool resolve_field(const package::light& light, std::string_view field, value& out) const noexcept;

    // Bearing accumulated along the `relative_to` chain, in degrees.
    [[nodiscard]] float absolute_bearing(const package::light& light, int depth) const noexcept;

    // Elevation, following `same_as` to whichever light actually declares one.
    [[nodiscard]] float effective_elevation(const package::light& light, int depth) const noexcept;

    std::shared_ptr<const package::manifest> m_sky;
    std::vector<knob> m_knobs;
};

// One loaded sky: what its author wrote, and what the player has since moved.
//
// Two different things with two different lifetimes, which is why they are two members rather than
// one mutable document. The manifest is reread whenever a file under the package changes; the values
// survive that, and are saved to the player's own settings file next to the plugin - never back into
// the package, which may be an archive and in any case belongs to its author.
//
// Shared by every layer of the sky, and that sharing is the mechanism: there is exactly one answer
// to "where is the sun" per sky, and every layer reads it.
struct loaded_sky {
    std::shared_ptr<const package::manifest> sky;
    state values;

    // The package's own file or directory name. Identifies the sky everywhere outside the manifest:
    // the catalog entry, the settings file, and each layer program's id.
    std::string stem;

    // Which layer carries the shared knobs in the panel. The shared block belongs to no layer, but
    // the panel reads knobs off layers, so the first one loaded lists them. Recorded here rather
    // than decided per rebuild, so a recompile of that layer does not hand the job to another one
    // and draw every shared knob twice.
    std::string shared_owner;
};

// Evaluates each binding and writes it into `constants`. Bindings whose source no longer resolves
// are skipped, leaving whatever the layer's own parameters put there.
//
// Cold path by construction: called when a knob moves, when a layer is (re)built, and not per frame.
// The values it writes are shader constants, so the next draw picks them up with no recompile.
void apply_bindings(const state& values,
    std::span<const resolved_binding> bindings,
    std::span<float> constants,
    std::span<float> vertex_constants) noexcept;
} // namespace tw::skybox::shared
