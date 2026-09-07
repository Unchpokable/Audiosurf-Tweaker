#include "pch.hxx"

#include "skybox/sky_lua.hxx"

#include "plugin/diagnostics.hxx"

namespace
{
constexpr float k_to_radians = 3.14159265358979323846f / 180.f;

// How often the count hook runs. Small enough that a tight infinite loop is caught in milliseconds,
// large enough that the check is not itself the cost.
constexpr int k_hook_interval = 10000;

// Everything one call needs, reachable from a lua_CFunction.
//
// A file-local pointer rather than something carried in the Lua state: LuaJIT is 5.1 and has no
// lua_getextraspace, and the registry alternative would mean pushing and popping inside the count
// hook, which runs at arbitrary points and has the least stack headroom of anywhere here.
//
// Correct because exactly one generator runs at a time - one sky, one VM, and today the render
// thread. If baking later moves to a worker this stays true (the VM cannot be entered from two
// threads anyway), but it is the assumption to check first if that changes.
struct call_state {
    const tw::skybox::lua::context* ctx {};
    std::vector<tw::skybox::lua::sprite>* out {};

    // The fill half. Exactly one of `ctx` and `fill` is set for any given call; the image functions
    // check for theirs and refuse politely when a script calls one from the wrong place.
    const tw::skybox::lua::fill_context* fill {};
    std::vector<tw::skybox::image::layer>* tiles {};

    // Every layer the script has made, owned here. Handles into this are what the script sees, so a
    // layer cannot be leaked, freed twice, or written past - LuaJIT does not bounds check cdata, and
    // handing over a raw buffer would make the rest of the sandbox beside the point.
    std::vector<tw::skybox::image::layer> layers;
    std::size_t texels {};

    std::chrono::steady_clock::time_point deadline {};
    bool over_budget {};
    bool over_count {};

    // xoshiro-style, seeded from the context. Its own rather than math.random because the layer has
    // to look the same every run or "did that change?" stops having an answer - and because a script
    // reaching for an unseeded generator would break that silently.
    std::uint64_t rng {};
};

call_state* g_active = nullptr;

call_state* state_of(lua_State* /*lua*/) noexcept
{
    return g_active;
}

float next_random(call_state& state) noexcept
{
    // splitmix64: one multiply-xor chain, no tables, deterministic, and good enough for placement.
    state.rng += 0x9E3779B97F4A7C15ULL;
    std::uint64_t z = state.rng;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    z = z ^ (z >> 31);

    return static_cast<float>(static_cast<double>(z >> 11) / 9007199254740992.0);
}

// The count hook. Aborts the script when its budget is spent - which is the only defence against a
// generator with an accidental `while true do end`, since nothing else on this thread would ever run
// again to notice.
void time_hook(lua_State* lua, lua_Debug* /*ar*/)
{
    call_state* state = state_of(lua);
    if(state == nullptr) {
        return;
    }

    if(std::chrono::steady_clock::now() < state->deadline) {
        return;
    }

    state->over_budget = true;

    // luaL_error longjmps out of the script. Nothing in this file holds a non-trivial local across
    // it - the sprite vector lives in the caller's frame, not here - which is what makes that safe
    // in a C++ translation unit (lua-scripting.md §2.3).
    luaL_error(lua, "script exceeded its time budget");
}

int push_sprite(lua_State* lua, std::array<float, 3> dir, float roll, float half_w, float half_h, int tile)
{
    call_state* state = state_of(lua);
    if(state == nullptr || state->out == nullptr || state->ctx == nullptr) {
        return luaL_error(lua, "tw.emit can only be called from place()");
    }

    if(static_cast<int>(state->out->size()) >= state->ctx->max_sprites) {
        state->over_count = true;
        return luaL_error(lua, "emitted more than %d sprites", state->ctx->max_sprites);
    }

    const float length = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);

    // A direction that is not a direction is dropped rather than normalised into something arbitrary:
    // a NaN here would become a quad with NaN vertices, and one of those takes the whole draw call
    // down with it on some drivers.
    if(!std::isfinite(length) || length <= 1e-6f) {
        return 0;
    }

    tw::skybox::lua::sprite emitted;
    emitted.dir = { dir[0] / length, dir[1] / length, dir[2] / length };
    emitted.roll = std::isfinite(roll) ? roll : 0.f;
    emitted.size = { std::isfinite(half_w) ? half_w : 0.f, std::isfinite(half_h) ? half_h : 0.f };
    emitted.tile = tile;

    state->out->push_back(emitted);

    return 0;
}

int l_emit(lua_State* lua)
{
    const std::array<float, 3> dir {
        static_cast<float>(luaL_checknumber(lua, 1)),
        static_cast<float>(luaL_checknumber(lua, 2)),
        static_cast<float>(luaL_checknumber(lua, 3)),
    };

    const auto roll = static_cast<float>(luaL_optnumber(lua, 4, 0.0));
    const auto half_w = static_cast<float>(luaL_optnumber(lua, 5, 0.05));
    const auto half_h = static_cast<float>(luaL_optnumber(lua, 6, -1.0));
    const int tile = static_cast<int>(luaL_optinteger(lua, 7, 0));

    return push_sprite(lua, dir, roll, half_w, half_h < 0.f ? half_w : half_h, tile);
}

// The form a person actually writes. A sky is naturally described in degrees of azimuth and
// elevation, and making the script convert to a vector itself would mean every script carrying the
// same six lines of trigonometry - and getting the axis convention wrong once each.
int l_emit_at(lua_State* lua)
{
    const auto azimuth = static_cast<float>(luaL_checknumber(lua, 1)) * k_to_radians;
    const auto elevation = static_cast<float>(luaL_checknumber(lua, 2)) * k_to_radians;
    const auto roll = static_cast<float>(luaL_optnumber(lua, 3, 0.0)) * k_to_radians;
    const auto half_w = static_cast<float>(luaL_optnumber(lua, 4, 3.0)) * k_to_radians;
    const auto half_h = static_cast<float>(luaL_optnumber(lua, 5, -1.0));
    const int tile = static_cast<int>(luaL_optinteger(lua, 6, 0));

    const float horizontal = std::cos(elevation);
    const std::array<float, 3> dir {
        std::sin(azimuth) * horizontal,
        std::sin(elevation),
        std::cos(azimuth) * horizontal,
    };

    return push_sprite(lua, dir, roll, half_w, half_h < 0.f ? half_w : half_h * k_to_radians, tile);
}

// A knob of this layer, by the id the manifest gave it.
//
// An unknown name is an error rather than nil, and that is the contract: a script asking for a knob
// means the author intends it to exist, so a missing one is a manifest that has fallen out of step
// with its own script. `place()` only ever runs at (re)build time, so this error *is* a load error -
// it surfaces in the Skybox tab the same as a shader that will not compile.
int l_prop(lua_State* lua)
{
    const char* name = luaL_checkstring(lua, 1);
    const call_state* state = state_of(lua);

    if(state == nullptr || name == nullptr) {
        return luaL_error(lua, "tw.prop called outside a generator");
    }

    // The same knobs are visible from both calls: they belong to the layer, not to whichever of its
    // two functions is running.
    const std::span<const tw::skybox::sky_param> params = state->ctx != nullptr ? state->ctx->params : state->fill->params;

    for(const tw::skybox::sky_param& param : params) {
        if(param.settings_id == name || param.property == name) {
            lua_pushnumber(lua, param.value[0]);
            return 1;
        }
    }

    return luaL_error(lua, "this layer declares no knob called '%s' - add it to the manifest", name);
}

int l_seed(lua_State* lua)
{
    const call_state* state = state_of(lua);

    lua_Integer seed = 0;
    if(state != nullptr) {
        seed = static_cast<lua_Integer>(state->ctx != nullptr ? state->ctx->seed : state->fill->seed);
    }

    lua_pushinteger(lua, seed);

    return 1;
}

int l_random(lua_State* lua)
{
    call_state* state = state_of(lua);
    if(state == nullptr) {
        lua_pushnumber(lua, 0.0);
        return 1;
    }

    const float value = next_random(*state);

    // tw.random(), tw.random(n) and tw.random(a, b), matching math.random so the shape is familiar.
    if(lua_gettop(lua) == 0) {
        lua_pushnumber(lua, value);
        return 1;
    }

    const lua_Integer low = lua_gettop(lua) >= 2 ? luaL_checkinteger(lua, 1) : 1;
    const lua_Integer high = lua_gettop(lua) >= 2 ? luaL_checkinteger(lua, 2) : luaL_checkinteger(lua, 1);

    if(high < low) {
        return luaL_error(lua, "empty range");
    }

    lua_pushinteger(lua, low + static_cast<lua_Integer>(value * static_cast<float>(high - low + 1)));

    return 1;
}

// ---------------------------------------------------------------------------------------------
// fill(): image layers
//
// Every function here takes handles rather than anything the script could hold on to, and an options
// table rather than a positional argument list - a composite has eight of them and nobody remembers
// the order.
// ---------------------------------------------------------------------------------------------

float opt_number(lua_State* lua, int table, const char* key, float fallback)
{
    if(!lua_istable(lua, table)) {
        return fallback;
    }

    lua_getfield(lua, table, key);
    const float value = lua_isnumber(lua, -1) ? static_cast<float>(lua_tonumber(lua, -1)) : fallback;
    lua_pop(lua, 1);

    return value;
}

int opt_int(lua_State* lua, int table, const char* key, int fallback)
{
    return static_cast<int>(opt_number(lua, table, key, static_cast<float>(fallback)));
}

std::string opt_string(lua_State* lua, int table, const char* key)
{
    if(!lua_istable(lua, table)) {
        return {};
    }

    lua_getfield(lua, table, key);
    std::string value = lua_isstring(lua, -1) != 0 ? lua_tostring(lua, -1) : std::string {};
    lua_pop(lua, 1);

    return value;
}

// Which channel an option names. Accepts "r"/"g"/"b"/"a" as well as an index, because a script says
// what it means more clearly with a letter and the engine does not care either way.
int opt_channel(lua_State* lua, int table, const char* key, int fallback)
{
    const std::string name = opt_string(lua, table, key);

    if(!name.empty()) {
        const std::size_t at = std::string_view { "rgba" }.find(name.front());
        return at == std::string_view::npos ? fallback : static_cast<int>(at);
    }

    return std::clamp(opt_int(lua, table, key, fallback), 0, 3);
}

tw::skybox::image::blend opt_blend(lua_State* lua, int table)
{
    using tw::skybox::image::blend;

    const std::string name = opt_string(lua, table, "mode");

    if(name == "add") {
        return blend::add;
    }
    if(name == "multiply") {
        return blend::multiply;
    }
    if(name == "screen") {
        return blend::screen;
    }
    if(name == "max") {
        return blend::max_of;
    }
    if(name == "replace") {
        return blend::replace;
    }

    return blend::normal;
}

// The layer a handle names, or null. Handles are 1-based so that a forgotten return value - nil,
// which converts to 0 - is an error rather than a silent write to the first layer.
tw::skybox::image::layer* layer_of(call_state* state, int handle) noexcept
{
    if(state == nullptr || handle < 1 || handle > static_cast<int>(state->layers.size())) {
        return nullptr;
    }

    return &state->layers[static_cast<std::size_t>(handle - 1)];
}

int push_layer(lua_State* lua, call_state& state, tw::skybox::image::layer&& made)
{
    if(!made.valid()) {
        return luaL_error(lua, "layer dimensions must be positive");
    }

    const std::size_t texels = static_cast<std::size_t>(made.width) * made.height;

    if(state.texels + texels > state.fill->max_texels) {
        return luaL_error(lua, "this script is holding more image data than it is allowed to (%d texels)",
            static_cast<int>(state.fill->max_texels));
    }

    state.texels += texels;
    state.layers.push_back(std::move(made));

    lua_pushinteger(lua, static_cast<lua_Integer>(state.layers.size()));

    return 1;
}

call_state* fill_state(lua_State* lua, const char* who)
{
    call_state* state = state_of(lua);

    if(state == nullptr || state->fill == nullptr) {
        luaL_error(lua, "%s can only be called from fill()", who);
        return nullptr;
    }

    return state;
}

int l_layer(lua_State* lua)
{
    call_state* state = fill_state(lua, "tw.layer");
    if(state == nullptr) {
        return 0;
    }

    const int width = static_cast<int>(luaL_optinteger(lua, 1, state->fill->size));
    const int height = static_cast<int>(luaL_optinteger(lua, 2, width));

    return push_layer(lua, *state, tw::skybox::image::make(width, height));
}

int l_load(lua_State* lua)
{
    call_state* state = fill_state(lua, "tw.load");
    if(state == nullptr) {
        return 0;
    }

    const char* name = luaL_checkstring(lua, 1);

    std::vector<std::byte> bytes;
    if(state->fill->files == nullptr || !state->fill->files->read(name, bytes)) {
        // One message for "outside the package", "missing" and "unreadable" alike. A script must not
        // be able to map the filesystem outside its own package by watching which error it gets.
        return luaL_error(lua, "cannot read '%s' from this package", name);
    }

    tw::skybox::image::layer decoded;
    if(!tw::skybox::image::decode(bytes, decoded)) {
        return luaL_error(lua, "'%s' is not an image this build can decode", name);
    }

    return push_layer(lua, *state, std::move(decoded));
}

int l_resize(lua_State* lua)
{
    call_state* state = fill_state(lua, "tw.resize");
    if(state == nullptr) {
        return 0;
    }

    const tw::skybox::image::layer* source = layer_of(state, static_cast<int>(luaL_checkinteger(lua, 1)));
    if(source == nullptr) {
        return luaL_error(lua, "tw.resize: not a layer");
    }

    const int width = static_cast<int>(luaL_checkinteger(lua, 2));
    const int height = static_cast<int>(luaL_optinteger(lua, 3, width));

    // Copied before push_layer, which may reallocate the vector `source` points into.
    tw::skybox::image::layer resized = tw::skybox::image::resize(*source, width, height);

    return push_layer(lua, *state, std::move(resized));
}

int l_composite(lua_State* lua)
{
    call_state* state = fill_state(lua, "tw.composite");
    if(state == nullptr) {
        return 0;
    }

    const int dst_handle = static_cast<int>(luaL_checkinteger(lua, 1));
    const int src_handle = static_cast<int>(luaL_checkinteger(lua, 2));

    if(dst_handle == src_handle) {
        return luaL_error(lua, "tw.composite: a layer cannot be composited onto itself");
    }

    tw::skybox::image::layer* dst = layer_of(state, dst_handle);
    const tw::skybox::image::layer* src = layer_of(state, src_handle);

    if(dst == nullptr || src == nullptr) {
        return luaL_error(lua, "tw.composite: not a layer");
    }

    tw::skybox::image::composite_options options;
    options.x = opt_number(lua, 3, "x", static_cast<float>(dst->width) * 0.5f);
    options.y = opt_number(lua, 3, "y", static_cast<float>(dst->height) * 0.5f);
    options.scale = opt_number(lua, 3, "scale", 1.f);
    options.rotation = opt_number(lua, 3, "rotation", 0.f);
    options.opacity = opt_number(lua, 3, "opacity", 1.f);
    options.mode = opt_blend(lua, 3);

    const std::string channels = opt_string(lua, 3, "channels");
    if(!channels.empty()) {
        options.channels = { false, false, false, false };
        for(const char c : channels) {
            const std::size_t at = std::string_view { "rgba" }.find(c);
            if(at != std::string_view::npos) {
                options.channels[at] = true;
            }
        }
    }

    tw::skybox::image::composite(*dst, *src, options);

    return 0;
}

int l_fbm(lua_State* lua)
{
    call_state* state = fill_state(lua, "tw.fbm");
    if(state == nullptr) {
        return 0;
    }

    tw::skybox::image::layer* target = layer_of(state, static_cast<int>(luaL_checkinteger(lua, 1)));
    if(target == nullptr) {
        return luaL_error(lua, "tw.fbm: not a layer");
    }

    tw::skybox::image::fbm_options options;
    options.octaves = opt_int(lua, 2, "octaves", options.octaves);
    options.frequency = opt_number(lua, 2, "frequency", options.frequency);
    options.warp = opt_number(lua, 2, "warp", options.warp);
    options.warp_frequency = opt_number(lua, 2, "warp_frequency", options.warp_frequency);
    options.seed = static_cast<unsigned int>(opt_number(lua, 2, "seed", static_cast<float>(state->fill->seed)));
    options.channel = opt_channel(lua, 2, "channel", options.channel);

    tw::skybox::image::fbm(*target, options);

    return 0;
}

int l_radial(lua_State* lua)
{
    call_state* state = fill_state(lua, "tw.radial");
    if(state == nullptr) {
        return 0;
    }

    tw::skybox::image::layer* target = layer_of(state, static_cast<int>(luaL_checkinteger(lua, 1)));
    if(target == nullptr) {
        return luaL_error(lua, "tw.radial: not a layer");
    }

    tw::skybox::image::radial_options options;
    options.inner = opt_number(lua, 2, "inner", options.inner);
    options.feather = opt_number(lua, 2, "feather", options.feather);
    options.depth = opt_number(lua, 2, "depth", options.depth);
    options.channel = opt_channel(lua, 2, "channel", options.channel);

    tw::skybox::image::radial(*target, options);

    return 0;
}

int l_normalize(lua_State* lua)
{
    call_state* state = fill_state(lua, "tw.normalize");
    if(state == nullptr) {
        return 0;
    }

    tw::skybox::image::layer* target = layer_of(state, static_cast<int>(luaL_checkinteger(lua, 1)));
    if(target == nullptr) {
        return luaL_error(lua, "tw.normalize: not a layer");
    }

    tw::skybox::image::normalize_options options;
    options.coverage = opt_number(lua, 2, "coverage", options.coverage);
    options.band = opt_number(lua, 2, "band", options.band);
    options.channel = opt_channel(lua, 2, "channel", options.channel);

    tw::skybox::image::normalize(*target, options);

    return 0;
}

int l_normals(lua_State* lua)
{
    call_state* state = fill_state(lua, "tw.normals");
    if(state == nullptr) {
        return 0;
    }

    const int dst_handle = static_cast<int>(luaL_checkinteger(lua, 1));
    const int height_handle = static_cast<int>(luaL_checkinteger(lua, 2));

    tw::skybox::image::layer* target = layer_of(state, dst_handle);
    const tw::skybox::image::layer* height = layer_of(state, height_handle);

    if(target == nullptr || height == nullptr) {
        return luaL_error(lua, "tw.normals: not a layer");
    }

    tw::skybox::image::normals_options options;
    options.span = opt_int(lua, 3, "span", options.span);
    options.relief = opt_number(lua, 3, "relief", options.relief);
    options.height_channel = opt_channel(lua, 3, "channel", options.height_channel);

    tw::skybox::image::normals(*target, *height, options);

    return 0;
}

int l_alpha(lua_State* lua)
{
    call_state* state = fill_state(lua, "tw.alpha");
    if(state == nullptr) {
        return 0;
    }

    tw::skybox::image::layer* target = layer_of(state, static_cast<int>(luaL_checkinteger(lua, 1)));
    const tw::skybox::image::layer* height = layer_of(state, static_cast<int>(luaL_checkinteger(lua, 2)));

    if(target == nullptr || height == nullptr) {
        return luaL_error(lua, "tw.alpha: not a layer");
    }

    tw::skybox::image::alpha_options options;
    options.softness = opt_number(lua, 3, "softness", options.softness);
    options.height_channel = opt_channel(lua, 3, "channel", options.height_channel);

    tw::skybox::image::alpha_from_height(*target, *height, options);

    return 0;
}

// Hands a finished layer back as one of the atlas tiles.
//
// Copied rather than moved out of the script's table: a script is free to keep composing onto the
// same layer and hand it back again under a different index, and a move would leave it holding an
// emptied one.
int l_tile(lua_State* lua)
{
    call_state* state = fill_state(lua, "tw.tile");
    if(state == nullptr) {
        return 0;
    }

    const int index = static_cast<int>(luaL_checkinteger(lua, 1));
    const tw::skybox::image::layer* source = layer_of(state, static_cast<int>(luaL_checkinteger(lua, 2)));

    if(source == nullptr) {
        return luaL_error(lua, "tw.tile: not a layer");
    }

    if(index < 0 || index >= state->fill->tiles) {
        return luaL_error(lua, "tile %d is outside the %d this layer declares", index, state->fill->tiles);
    }

    const int size = state->fill->size;

    // Resized here rather than refused, because "the tile is 256 square" is the engine's packing
    // requirement and not something an author composing at 512 should have to track.
    (*state->tiles)[static_cast<std::size_t>(index)]
        = (source->width == size && source->height == size) ? *source : tw::skybox::image::resize(*source, size, size);

    return 0;
}

int l_log(lua_State* lua)
{
    const char* text = luaL_optstring(lua, 1, "");
    TW_LOG_INFO("sky_lua: {}", text != nullptr ? text : "");

    return 0;
}

// Refuses every write. A placement script reading the sky's lights must not be able to move them -
// and a write that silently did nothing would be worse than an error, because the author would go
// looking for the reason their sun did not move.
int l_readonly_newindex(lua_State* lua)
{
    return luaL_error(lua, "the lights table is read-only - a generator reads the sky's lights, it does not move them");
}

// Replaces the table on top of the stack with an empty proxy that reads through to it and refuses to
// be written.
//
// The proxy has to be *empty*, and that is the whole subtlety. `__newindex` fires only for keys the
// table does not already have, so hanging it on the data table itself protects nothing: every field a
// script would want to assign to is one that already exists, and the assignment goes straight past
// the metatable. Measured, not reasoned about - the first version of this let `lights.primary.bearing
// = 99` succeed silently, which is precisely the failure it was written to prevent.
//
// With an empty proxy every read misses and goes through `__index`, and every write is a new key and
// goes through `__newindex`. `__metatable` is set so the proxy cannot simply be unwrapped again.
void wrap_readonly(lua_State* lua)
{
    lua_createtable(lua, 0, 0); // the proxy
    lua_createtable(lua, 0, 3); // its metatable

    lua_pushvalue(lua, -3); // the data table
    lua_setfield(lua, -2, "__index");

    lua_pushcfunction(lua, &l_readonly_newindex);
    lua_setfield(lua, -2, "__newindex");

    lua_pushboolean(lua, 0);
    lua_setfield(lua, -2, "__metatable");

    lua_setmetatable(lua, -2);

    // proxy on top, data table beneath it: drop the data table and leave the proxy in its place.
    lua_replace(lua, -2);
}

// Mirrors the sky's shared block into a plain table: lights.primary.direction = {x, y, z}.
//
// The shared *block* rather than the layer's `bind` map, which answers a different question - "which
// register receives this" - and means nothing to a script. Every path sky_shared resolves is offered
// here, authored and derived alike, so a script asks for a direction without knowing that its author
// typed a bearing.
void push_lights(lua_State* lua, const tw::skybox::shared::state& values)
{
    static constexpr std::array<std::string_view, 6> k_fields {
        "bearing",
        "elevation",
        "color",
        "intensity",
        "direction",
        "radiance",
    };

    const tw::skybox::package::manifest* sky = values.manifest();

    lua_createtable(lua, 0, sky == nullptr ? 0 : static_cast<int>(sky->lights.size()));

    if(sky != nullptr) {
        for(const tw::skybox::package::light& light : sky->lights) {
            lua_createtable(lua, 0, static_cast<int>(k_fields.size()) + static_cast<int>(light.extra.size()));

            const auto add_field = [&](std::string_view field) {
                std::string path = "lights." + light.id + "." + std::string { field };

                tw::skybox::shared::value resolved {};
                if(!values.resolve(path, resolved)) {
                    return;
                }

                if(resolved.count == 1) {
                    lua_pushnumber(lua, resolved.data[0]);
                }
                else {
                    lua_createtable(lua, resolved.count, 0);
                    for(int i = 0; i < resolved.count; ++i) {
                        lua_pushnumber(lua, resolved.data[static_cast<std::size_t>(i)]);
                        lua_rawseti(lua, -2, i + 1);
                    }
                }

                lua_setfield(lua, -2, std::string { field }.c_str());
            };

            for(const std::string_view field : k_fields) {
                add_field(field);
            }

            // The author's own fields, on the same footing - `lights.primary.sun_oreol_radius` is
            // read exactly like `lights.primary.intensity`.
            for(const tw::skybox::package::param& field : light.extra) {
                add_field(field.id);
            }

            wrap_readonly(lua);
            lua_setfield(lua, -2, light.id.c_str());
        }
    }

    wrap_readonly(lua);
    lua_setglobal(lua, "lights");
}

// What the script keeps of the standard library.
//
// Cut far harder than tw::lua::host's sandbox, and it can be: a generator computes numbers and emits
// sprites. It has no reason to open a file, load a chunk, reach the engine, or know what time it is -
// and `os.time` in particular would be a way around the seed, which is what makes a sky reproducible.
void strip_sandbox(lua_State* lua) noexcept
{
    static constexpr std::array<const char*, 14> k_remove {
        "ffi",
        "io",
        "os",
        "package",
        "require",
        "dofile",
        "loadfile",
        "load",
        "loadstring",
        "debug",
        "jit",
        "newproxy",
        "collectgarbage",
        "print", // tw.log instead, so script output reaches the plugin's log rather than nowhere
    };

    for(const char* name : k_remove) {
        lua_pushnil(lua);
        lua_setglobal(lua, name);
    }

    // math survives, minus its own generator: a script calling math.random would produce a sky that
    // differs between runs *and* between machines, which is exactly what tw.random exists to prevent.
    lua_getglobal(lua, "math");
    if(lua_istable(lua, -1)) {
        for(const char* field : { "random", "randomseed" }) {
            lua_pushnil(lua);
            lua_setfield(lua, -2, field);
        }
    }
    lua_pop(lua, 1);
}

void install_api(lua_State* lua)
{
    lua_createtable(lua, 0, 6);

    const auto add = [lua](const char* name, lua_CFunction fn) {
        lua_pushcfunction(lua, fn);
        lua_setfield(lua, -2, name);
    };

    add("emit", &l_emit);
    add("emit_at", &l_emit_at);
    add("prop", &l_prop);
    add("seed", &l_seed);
    add("random", &l_random);
    add("log", &l_log);

    // fill() only. Registered unconditionally because the alternative - two API tables depending on
    // which call is running - would let a script look correct and fail at exactly one of them.
    add("layer", &l_layer);
    add("load", &l_load);
    add("resize", &l_resize);
    add("composite", &l_composite);
    add("fbm", &l_fbm);
    add("radial", &l_radial);
    add("normalize", &l_normalize);
    add("normals", &l_normals);
    add("alpha", &l_alpha);
    add("tile", &l_tile);

    lua_setglobal(lua, "tw");
}

// One compiled generator, kept so that a rebuild costs a call rather than a compile.
struct loaded_script {
    lua_State* lua {};
    std::string name;
    std::filesystem::file_time_type write_time {};
    std::string error;
};

loaded_script g_script;

void close_script() noexcept
{
    if(g_script.lua != nullptr) {
        lua_close(g_script.lua);
    }

    g_script = {};
}

int traceback_handler(lua_State* lua)
{
    const char* message = lua_tostring(lua, 1);
    luaL_traceback(lua, lua, message != nullptr ? message : "error", 1);

    return 1;
}

// Compiles the script into a fresh state. A fresh one per load rather than a reused one, because a
// reload is exactly when leftover globals from the previous version would be most confusing.
bool load_script(const tw::skybox::package::file_system* files, std::string_view name, const char* entry, std::string& error)
{
    std::string source;
    if(files == nullptr || !files->read_text(name, source)) {
        error = "could not read " + std::string { name };
        return false;
    }

    // Through the package rather than off a path: an archived package has no paths, and keying this
    // cache on one would have every script in a zip share the empty key.
    const std::filesystem::file_time_type stamp = files->write_time(name);

    if(g_script.lua != nullptr && g_script.name == name && stamp != std::filesystem::file_time_type {} && g_script.write_time == stamp) {
        return g_script.error.empty() ? true : (error = g_script.error, false);
    }

    close_script();

    lua_State* lua = luaL_newstate();
    if(lua == nullptr) {
        error = "out of memory";
        return false;
    }

    luaL_openlibs(lua);

    // JIT off, and this is not a performance oversight - it is what makes the time budget real.
    //
    // LuaJIT calls debug hooks from the interpreter only. A loop that gets traced and compiled runs
    // with no hook at all, so `while true do end` - which is precisely the shape LuaJIT traces first -
    // spins forever with the count hook installed and never fires it. Measured: the harness pinned a
    // core for five minutes before it was killed.
    //
    // The cost is real but affordable, and worth stating as a number rather than a hope: the shipped
    // cloud generator places 220 sprites in 2.8 ms interpreted. That is once per *rebuild*, not per
    // frame - so it is invisible except while a slider is actually being dragged, when it lands on
    // top of the vertex buffer rebuild that drag was already paying for.
    //
    // Against that: without this line the sandbox's only defence against a runaway script does not
    // work at all. If a generator ever gets heavy enough for the drag to matter, the answer is to
    // rebuild off the drag rather than to hand a script an unbounded loop.
    luaJIT_setmode(lua, 0, LUAJIT_MODE_ENGINE | LUAJIT_MODE_OFF);

    install_api(lua);
    strip_sandbox(lua);

    const std::string chunk_name = "@" + std::string { name };

    if(luaL_loadbuffer(lua, source.data(), source.size(), chunk_name.c_str()) != 0) {
        error = lua_tostring(lua, -1) != nullptr ? lua_tostring(lua, -1) : "could not be compiled";
        lua_close(lua);

        g_script.name = name;
        g_script.write_time = stamp;
        g_script.error = error;

        return false;
    }

    // Runs the file body, which is what defines `place`. A script whose top level errors is a script
    // that never gets to declare anything, so this is reported the same as a compile failure.
    if(lua_pcall(lua, 0, 0, 0) != 0) {
        error = lua_tostring(lua, -1) != nullptr ? lua_tostring(lua, -1) : "failed to run";
        lua_close(lua);

        g_script.name = name;
        g_script.write_time = stamp;
        g_script.error = error;

        return false;
    }

    lua_getglobal(lua, entry);
    const bool has_entry = lua_isfunction(lua, -1);
    lua_pop(lua, 1);

    if(!has_entry) {
        error = std::string { name } + " declares no " + entry + "() function";
        lua_close(lua);

        g_script.name = name;
        g_script.write_time = stamp;
        g_script.error = error;

        return false;
    }

    g_script.lua = lua;
    g_script.name = name;
    g_script.write_time = stamp;
    g_script.error.clear();

    return true;
}
} // namespace

namespace tw::skybox::lua
{
place_result run_place(std::string_view script, const context& ctx)
{
    place_result out;

    if(!load_script(ctx.files, script, "place", out.error)) {
        return out;
    }

    lua_State* lua = g_script.lua;

    call_state state;
    state.ctx = &ctx;
    state.out = &out.sprites;
    state.deadline = std::chrono::steady_clock::now() + ctx.budget;
    state.rng = ctx.seed;

    g_active = &state;

    // Rebuilt per call: a knob may have moved since the last one, and a script that read a stale
    // light would be lit by where the sun was rather than where it is.
    if(ctx.values != nullptr) {
        push_lights(lua, *ctx.values);
    }

    lua_sethook(lua, &time_hook, LUA_MASKCOUNT, k_hook_interval);

    lua_pushcfunction(lua, &traceback_handler);
    const int handler = lua_gettop(lua);

    lua_getglobal(lua, "place");

    const int status = lua_pcall(lua, 0, 0, handler);

    lua_sethook(lua, nullptr, 0, 0);
    g_active = nullptr;

    if(status != 0) {
        const char* message = lua_tostring(lua, -1);
        out.error = message != nullptr ? message : "place() failed";
        lua_pop(lua, 1);

        out.sprites.clear();
    }

    lua_pop(lua, 1); // the traceback handler

    if(out.error.empty() && out.sprites.empty()) {
        out.error = "place() emitted no sprites";
    }

    return out;
}

fill_result run_fill(std::string_view script, const fill_context& ctx)
{
    fill_result out;

    if(!load_script(ctx.files, script, "fill", out.error)) {
        return out;
    }

    lua_State* lua = g_script.lua;

    // Sized and blanked up front, so a script that skips a tile leaves an empty one rather than a
    // short list the caller would have to index carefully.
    out.tiles.resize(static_cast<std::size_t>((std::max)(ctx.tiles, 1)));

    call_state state;
    state.fill = &ctx;
    state.tiles = &out.tiles;
    state.deadline = std::chrono::steady_clock::now() + ctx.budget;
    state.rng = ctx.seed;

    g_active = &state;

    lua_sethook(lua, &time_hook, LUA_MASKCOUNT, k_hook_interval);

    lua_pushcfunction(lua, &traceback_handler);
    const int handler = lua_gettop(lua);

    lua_getglobal(lua, "fill");

    // The tile count and size, so the common loop reads `for i = 0, tiles - 1` without the script
    // having to know they are also available through tw.
    lua_pushinteger(lua, ctx.tiles);
    lua_pushinteger(lua, ctx.size);

    const int status = lua_pcall(lua, 2, 0, handler);

    lua_sethook(lua, nullptr, 0, 0);
    g_active = nullptr;

    if(status != 0) {
        const char* message = lua_tostring(lua, -1);
        out.error = message != nullptr ? message : "fill() failed";
        lua_pop(lua, 1);

        out.tiles.clear();
    }

    lua_pop(lua, 1); // the traceback handler

    // Every tile has to arrive. A missing one would be drawn as a transparent hole on whichever
    // sprites happened to draw it, which reads as a rendering bug rather than as an unfinished script.
    if(out.error.empty()) {
        for(std::size_t i = 0; i < out.tiles.size(); ++i) {
            if(!out.tiles[i].valid()) {
                out.error = "fill() did not produce tile " + std::to_string(i) + " of " + std::to_string(ctx.tiles);
                out.tiles.clear();
                break;
            }
        }
    }

    return out;
}

void reset() noexcept
{
    close_script();
}
} // namespace tw::skybox::lua
