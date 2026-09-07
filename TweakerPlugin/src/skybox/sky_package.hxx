#pragma once

#include "skybox/sky_vfs.hxx"

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

    // Fields the author invented: "sun_oreol_radius", "flare_width", whatever this sky's shaders and
    // scripts happen to need from a light. Each is an ordinary knob, bound and read by the same paths
    // as the four above - `lights.primary.sun_oreol_radius` resolves exactly like
    // `lights.primary.intensity`.
    //
    // Here rather than in `shared.values` because they belong *to the light*: a sky with two suns
    // wants two halo radii, and flattening them into loose values would mean naming them
    // "primary_halo" and "twin_halo" by hand and hoping the two stay in step. A light is a bundle of
    // named numbers of which four happen to be well-known.
    std::vector<param> extra;

    // The well-known field names, which an author's own field may not shadow. Two of them are
    // *derived* (see shared::state), so allowing a knob by the same name would give one path two
    // answers - one stored, one computed - with nothing to say which wins.
    [[nodiscard]] static bool is_reserved_field(std::string_view name) noexcept
    {
        return name == "id" || name == "kind" || name == "bearing" || name == "elevation" || name == "color"
            || name == "intensity" || name == "strength" || name == "direction" || name == "radiance";
    }
};

enum class layer_kind {
    unknown,
    fullsky, // a pixel shader painting the whole cube - what the sky has always been
    sprites, // the geometry layer's quads
};

// Where a geometry layer's texture comes from.
//
// Declared by the manifest rather than returned by the script, and that is the point. A `fill()` that
// handed back "a texture or a shader, plus a flag saying which" would put the answer to "what is this
// layer made of" behind running the author's code - so a package could only be checked for
// completeness by executing it, and the same question would have two homes: the manifest's fallback
// and the script's flag. The manifest decides; the script only produces data.
enum class fill_kind {
    builtin, // the plugin's own cloud bake - what a layer with no `fill` block gets
    atlas,   // a script bakes the tiles: fill() is called and must hand every one of them back
    texture, // a ready-made image shipped in the package
    shader,  // nothing is baked; the pixel shader paints the sprite itself
};

struct fill {
    fill_kind kind { fill_kind::builtin };

    std::string generator; // kind == atlas
    std::string path;      // kind == texture

    // How many distinct sprite images, and how big each is. The grid they are laid out in is the
    // engine's business; a script is handed tiles by index and never sees the packing.
    int tiles { 4 };
    int size { 256 };
};

// How a texture is addressed outside 0..1, and how it is filtered between texels. Both are sampler
// state rather than anything about the file, which is why they are declared here and not baked in.
enum class texture_address {
    wrap,   // what a tiling field wants - and a baked noise volume is one
    clamp,  // what a gradient ramp or a single sprite wants
    mirror, // occasionally what a hand-painted tile wants
};

enum class texture_filter {
    linear, // trilinear between texels, and between mip levels when the file has them
    point,  // no interpolation at all - for a lookup table whose entries mean something discrete
};

// One texture a layer's shader samples, bound to one sampler register.
//
// The *shape* - 2D, volume or cube - is deliberately not declared here. The .dds file already says
// which it is, and a manifest that said so too would be a second source of truth able to disagree
// with the first; the loader reads it from the header and the diagnostics report what it found.
struct texture_ref {
    // Relative to the package root, as every other path in the manifest is.
    std::string path;

    // Which sampler register the shader declares it at: `sampler3D s_noise : register(s0)` is 0.
    //
    // A number rather than a name, because ps_3_0 bytecode carries no sampler names to match against
    // - the constant table names float registers only. Getting this wrong binds the texture where
    // the shader is not looking, which is why the manifest has to say it out loud.
    int slot {};

    texture_address address { texture_address::wrap };
    texture_filter filter { texture_filter::linear };

    // A label for the overlay and for diagnostics. Optional; the path stands in when it is empty.
    std::string name;
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

    // A Lua generator that places this layer's geometry, relative to the package root. Empty means
    // the built-in generator does it.
    //
    // This is the authorial half of a geometry layer. The engine keeps the billboard basis, the pole
    // handling and the vertex format - invariants rather than choices - and the script gets the one
    // question that is genuinely the author's: where the clouds are. See sky_lua.
    std::string generator;

    // What `tw.seed()` answers with, and therefore whether this layer looks the same every run.
    //
    // Declared here rather than decided by the script, and that is deliberate. The script's own code
    // is identical either way - it always asks the engine - so the choice stays *data*, which means
    // it can be overridden while somebody is working on the sky. That matters more than it sounds:
    // with a fresh seed per rebuild, "did my edit change anything?" has no answer, so being able to
    // pin it is the difference between tuning a generator and guessing at one.
    //
    // Fixed by default. A sky that surprises you on every load should be something its author asked
    // for out loud.
    unsigned int seed { 0x5EED1234U };
    bool seed_per_build {}; // "seed": "random"

    // What this layer's sprites are painted with. See fill_kind.
    struct fill fill;

    // Textures the layer's own shader samples. Any layer kind: a `fullsky` shader that reads a baked
    // noise volume is the case this was added for, but a gradient ramp, a star chart or a cloud
    // cover mask are the same mechanism.
    //
    // Empty for every sky written before this existed, which is what it has to be: a layer that
    // declares no texture binds none and behaves exactly as it did.
    std::vector<texture_ref> textures;

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

    // How its files are read. Holds the confinement rule and, later, the difference between a
    // directory and an archive - see sky_vfs. Copyable, and carried on the manifest so that anything
    // holding the document can read the package it came from without being handed a second thing.
    file_system files;

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

// The same, out of a zip. The two forms differ only in where the bytes come from - see sky_vfs.
[[nodiscard]] manifest load_archive(const std::filesystem::path& file);

// Whichever of the two `path` is. What callers should use unless they know which form they have:
// which one a `.sky` is is the author's packaging choice, not a different kind of sky.
[[nodiscard]] manifest load(const std::filesystem::path& path);

// The newest write time across `watched`, or the epoch when there is nothing to watch. Cheap enough
// for the same rate-limited poll the single-file shader reload already uses.
[[nodiscard]] std::filesystem::file_time_type newest_write_time(const manifest& package) noexcept;

[[nodiscard]] std::string_view kind_name(layer_kind kind) noexcept;
} // namespace tw::skybox::package
