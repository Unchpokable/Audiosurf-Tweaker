#pragma once

// The `.sky` package: one sky's layers, shaders, assets and every parameter, in one manifest.
//
// See Docs/Internal/sky-package.md for the format and for why it exists. The short version is that
// "configuration lives in a comment inside the HLSL" was right while a sky was *one* pixel shader,
// and stopped being right the moment a second layer needed to be lit by the first one's sun: there
// was nothing to bind them with except a chain of adapters.
//
// Two forms of the same package, one loader:
//
//   OurDraftsCollides.sky/   - a directory. The development form, because the loop that made the
//                              shader work fast is "save the .hlsl, alt-tab, look", and re-zipping
//                              for a one-character change destroys it.
//   OurDraftsCollides.sky    - the same directory zipped. The distribution form. Later.
//
// Nothing here touches a device or draws anything. It reads a manifest and reports what it found,
// including what it could not make sense of - a sky that half-loads has to say so rather than
// silently drawing three layers out of four.
namespace tw::skybox::package
{
// One knob, as the manifest declares it.
//
// A knob points at one of two things, and says which by the key it uses:
//
//   "var"  - a shader variable of the layer it belongs to. Deliberately not a register: register
//            numbers are fxc's to assign and they move whenever anything above them is edited, so a
//            manifest pointing at c7 would quietly drift from its own shader after a harmless edit.
//            The loader resolves names through the compiled shader's constant table - sky_bytecode.
//   "prop" - a property of the layer *kind* rather than of its shader. A sprite layer's sprite count
//            rebuilds a vertex buffer; no shader constant can express that, and pretending otherwise
//            would mean a second parameter system for exactly the knobs that need one least.
//
// A shared value (a light, or an entry under `shared.values`) uses neither: it belongs to the sky
// and is written into layers by their bindings, so it names nothing of its own.
struct param {
    std::string id;    // stable settings key, unique within its layer; the variable name by default
    std::string label; // what the overlay shows
    std::string group; // heading it files under, may be empty

    std::string variable; // "var": a shader variable of this layer
    std::string property; // "prop": a native property of this layer's kind

    int count { 1 }; // 1 = scalar slider, 3 = colour

    std::array<float, 3> value {};
    std::array<float, 3> default_value {};

    float min_value { 0.f };
    float max_value { 1.f };

    // Whether the author wrote this entry at all. A light declares only the fields it has - an
    // ambient fill has no bearing, a plain sun has no colour - and a default-constructed param is
    // otherwise indistinguishable from one the author set to zero.
    bool declared {};

    [[nodiscard]] bool is_color() const noexcept
    {
        return count == 3;
    }
};

enum class light_kind {
    // Has a direction, and everything lit by it agrees about where that is: a sun, a moon, a second
    // sun, the glow off a planet that fills half the sky.
    directional,

    // Has no direction. A sky fill, a ground bounce - the light that keeps the unlit side of a cloud
    // from being a hole. Asking one for a direction is an authoring mistake and is reported as one.
    ambient,
};

// A light the whole sky shares. Layers bind to it rather than each declaring their own, which is
// the point of the format: the sky owns its lights, and everything drawn in it agrees about where
// they are and what colour they are.
//
// Generalised from `suns` deliberately. Two suns was never the interesting number - a sky has a sun,
// a moon, an aurora, a horizon glow, and a cloud lit by only one of them is a cloud that visibly
// does not belong to the sky it hangs in. Nothing here is specific to a star: a light is a direction,
// a colour and an intensity, and any of the three may be absent.
//
// Two forms of the same thing, and both are bindable. `bearing`/`elevation` are what the author
// types; `direction` and `radiance` are derived from them and are what a lighting shader wants.
// That is what lets a cloud layer ask for a direction without knowing anything about how the sky's
// author prefers to write one down.
struct light {
    std::string id;
    light_kind kind { light_kind::directional };

    param bearing;
    param elevation;
    param color;
    param intensity;

    // Second lights are almost always authored *relative* to the first rather than with their own
    // bearing. Empty means the values above stand on their own.
    std::string bearing_relative_to;
    std::string elevation_same_as;
};

enum class layer_kind {
    unknown,
    fullsky, // a pixel shader painting the whole cube - what the sky has always been
    sprites, // the geometry layer's quads
};

// "put <source> into <variable>", where source names a shared value - "lights.primary.direction",
// "lights.moon.radiance", "values.haze". See sky_shared for the paths that resolve.
struct binding {
    std::string variable;
    std::string source;
};

struct layer {
    std::string id;
    layer_kind kind { layer_kind::unknown };

    // Relative to the package root. For `sprites` this is the stem of a .vs/.ps pair, and may be
    // empty - a sprite layer with no shader of its own draws with the one built into the plugin.
    std::string shader;

    std::vector<binding> bindings;
    std::vector<param> params;

    bool enabled { true };
};

struct manifest {
    int format {};

    std::string name;
    std::string author;

    // Layout generation. Folded into settings keys, as `@sky`'s `version` was - though it matters
    // far less here, because parameters are keyed by name rather than by register and a rename no
    // longer looks like a rewrite.
    int version {};

    // The sky's shared block: its lights, and any other value more than one layer has to agree
    // about. Both are addressed by the same path grammar and both are bindable - see sky_shared.
    std::vector<light> lights;
    std::vector<param> values;

    std::vector<layer> layers;

    // Where it was loaded from; every path in the manifest resolves against this.
    std::filesystem::path root;

    // Every file the manifest referred to, for the reload watch. A sky is stale when any of them is
    // newer than when it was read - the manifest alone is not enough, since the usual edit is to a
    // shader.
    std::vector<std::filesystem::path> watched;

    // Empty when everything parsed. Anything that did not is a line here and is shown in the Skybox
    // tab: a package that half-loads must say so rather than drawing three layers out of four and
    // leaving the author to guess.
    std::vector<std::string> diagnostics;

    [[nodiscard]] bool usable() const noexcept
    {
        return !layers.empty();
    }

    [[nodiscard]] const light* find_light(std::string_view id) const noexcept
    {
        for(const light& entry : lights) {
            if(entry.id == id) {
                return &entry;
            }
        }

        return nullptr;
    }

    [[nodiscard]] const layer* find_layer(std::string_view id) const noexcept
    {
        for(const layer& entry : layers) {
            if(entry.id == id) {
                return &entry;
            }
        }

        return nullptr;
    }
};

// Reads `<root>/Config.json`. Never throws and never partially fails silently: a manifest that
// cannot be read at all comes back with no layers and a diagnostic saying why.
[[nodiscard]] manifest load_directory(const std::filesystem::path& root);

// The newest write time across `watched`, or the epoch when there is nothing to watch. Cheap enough
// for the same rate-limited poll the single-file shader reload already uses.
[[nodiscard]] std::filesystem::file_time_type newest_write_time(const manifest& package) noexcept;

[[nodiscard]] std::string_view kind_name(layer_kind kind) noexcept;
} // namespace tw::skybox::package
