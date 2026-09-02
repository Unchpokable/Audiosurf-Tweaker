#include "pch.hxx"

#include "skybox/sky_package.hxx"

#include "plugin/diagnostics.hxx"

#include <libyyjson/yyjson.h>

namespace
{
constexpr std::string_view k_manifest_name = "Config.json";

// The format this loader understands. Bumped only for a change that would make an older loader
// misread a newer file, which is not the same as adding a field - an unknown key is ignored, and
// that is how most additions will arrive.
constexpr int k_supported_format = 1;

// Comments and trailing commas. Not JSON, deliberately allowed: a sky author annotating what a knob
// means is the normal case, and a format that forbids it invites a second file nobody keeps in step.
constexpr yyjson_read_flag k_read_flags = YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS;

// yyjson owns its document; this makes sure it is freed on every path out, including the early
// returns that make the parsing below readable.
struct document {
    yyjson_doc* doc {};

    ~document()
    {
        if(doc != nullptr) {
            yyjson_doc_free(doc);
        }
    }

    document() = default;
    document(const document&) = delete;
    document& operator=(const document&) = delete;
};

std::string_view as_string(yyjson_val* value, std::string_view fallback = {}) noexcept
{
    if(value == nullptr || !yyjson_is_str(value)) {
        return fallback;
    }

    return { yyjson_get_str(value), yyjson_get_len(value) };
}

float as_float(yyjson_val* value, float fallback) noexcept
{
    if(value == nullptr || !yyjson_is_num(value)) {
        return fallback;
    }

    return static_cast<float>(yyjson_get_num(value));
}

int as_int(yyjson_val* value, int fallback) noexcept
{
    if(value == nullptr || !yyjson_is_int(value)) {
        return fallback;
    }

    return static_cast<int>(yyjson_get_sint(value));
}

bool as_bool(yyjson_val* value, bool fallback) noexcept
{
    if(value == nullptr || !yyjson_is_bool(value)) {
        return fallback;
    }

    return yyjson_get_bool(value);
}

std::string_view member_string(yyjson_val* object, const char* key, std::string_view fallback = {}) noexcept
{
    return as_string(yyjson_obj_get(object, key), fallback);
}

// A default may be one number or three - a scalar knob or a colour - and which it is decides the
// control the overlay draws. Reading it is therefore also what sets `count`.
void read_default(yyjson_val* value, tw::skybox::package::param& out) noexcept
{
    if(value == nullptr) {
        return;
    }

    if(yyjson_is_num(value)) {
        out.count = 1;
        out.value[0] = static_cast<float>(yyjson_get_num(value));
        return;
    }

    if(!yyjson_is_arr(value)) {
        return;
    }

    out.count = 3;

    std::size_t index = 0;
    std::size_t max = 0;
    yyjson_val* element = nullptr;
    yyjson_arr_foreach(value, index, max, element) {
        if(index < out.value.size()) {
            out.value[index] = as_float(element, 0.f);
        }
    }
}

tw::skybox::package::param read_param(yyjson_val* object, std::string_view group)
{
    tw::skybox::package::param out;
    out.declared = true;

    out.variable.assign(member_string(object, "var"));
    out.property.assign(member_string(object, "prop"));
    out.label.assign(member_string(object, "label"));
    out.group.assign(member_string(object, "group", group));

    // The name it points at is the settings key unless the author names one. That is deliberate: a
    // variable can be renamed, and an author who cares about not losing saved values can pin an id
    // that outlives the rename. Neither is a register, which is what used to move on its own.
    out.id.assign(member_string(object, "id", out.variable.empty() ? out.property : out.variable));

    read_default(yyjson_obj_get(object, "default"), out);

    out.min_value = as_float(yyjson_obj_get(object, "min"), 0.f);
    out.max_value = as_float(yyjson_obj_get(object, "max"), 1.f);

    if(out.max_value <= out.min_value) {
        out.max_value = out.min_value + 1.f;
    }

    if(out.label.empty()) {
        out.label = out.variable.empty() ? out.id : out.variable;
    }

    out.default_value = out.value;

    return out;
}

tw::skybox::package::layer_kind parse_kind(std::string_view text) noexcept
{
    if(text == "fullsky") {
        return tw::skybox::package::layer_kind::fullsky;
    }
    if(text == "sprites") {
        return tw::skybox::package::layer_kind::sprites;
    }

    return tw::skybox::package::layer_kind::unknown;
}

// One entry of `shared.lights`.
//
// `suns` is accepted as a spelling of `lights` and `strength` as a spelling of `intensity`, because
// that is what the first packages were written with and because for a sky with two suns in it those
// are the better words. Neither is a second concept - the loader produces the same `light` either
// way, and everything downstream knows only the general one.
void read_light(yyjson_val* element, std::string_view group, tw::skybox::package::manifest& out)
{
    tw::skybox::package::light light;
    light.id.assign(member_string(element, "id"));

    if(light.id.empty()) {
        out.diagnostics.emplace_back("a light with no id was ignored - layers bind to lights by id");
        return;
    }

    if(out.find_light(light.id) != nullptr) {
        out.diagnostics.emplace_back("two lights are called '" + light.id + "' - the second was ignored");
        return;
    }

    const std::string_view kind = member_string(element, "kind", "directional");
    if(kind == "ambient") {
        light.kind = tw::skybox::package::light_kind::ambient;
    }
    else if(kind != "directional") {
        out.diagnostics.emplace_back(
            "light '" + light.id + "': unknown kind '" + std::string(kind) + "' - this build understands directional and ambient");
    }

    if(yyjson_val* bearing = yyjson_obj_get(element, "bearing"); bearing != nullptr) {
        light.bearing = read_param(bearing, group);
        light.bearing_relative_to.assign(member_string(bearing, "relative_to"));
    }

    if(yyjson_val* elevation = yyjson_obj_get(element, "elevation"); elevation != nullptr) {
        light.elevation = read_param(elevation, group);
        light.elevation_same_as.assign(member_string(elevation, "same_as"));

        // `same_as` declares a relation, not a value. Left as a knob it would put a nameless slider
        // at zero into the panel and let somebody move a number nothing reads.
        if(!light.elevation_same_as.empty()) {
            light.elevation.declared = false;
        }
    }

    if(yyjson_val* color = yyjson_obj_get(element, "color"); color != nullptr) {
        light.color = read_param(color, group);

        // A colour is three floats whatever the author wrote as its default, and reading a single
        // number as a scalar here would produce a slider where the panel has to draw a swatch.
        light.color.count = 3;
    }

    yyjson_val* intensity = yyjson_obj_get(element, "intensity");
    if(intensity == nullptr) {
        intensity = yyjson_obj_get(element, "strength");
    }
    if(intensity != nullptr) {
        light.intensity = read_param(intensity, group);
    }

    if(light.kind == tw::skybox::package::light_kind::ambient && (light.bearing.declared || light.elevation.declared)) {
        out.diagnostics.emplace_back("light '" + light.id + "' is ambient but declares a direction - the direction is ignored");
    }

    out.lights.push_back(std::move(light));
}

void read_shared(yyjson_val* shared, tw::skybox::package::manifest& out)
{
    for(const char* key : { "lights", "suns" }) {
        yyjson_val* array = yyjson_obj_get(shared, key);
        if(array == nullptr || !yyjson_is_arr(array)) {
            continue;
        }

        // "Suns" rather than "Lights" for the older spelling: the panel heading should read the way
        // the author's own file does.
        const std::string_view group = std::string_view { key } == "suns" ? "Suns" : "Lights";

        std::size_t index = 0;
        std::size_t max = 0;
        yyjson_val* element = nullptr;
        yyjson_arr_foreach(array, index, max, element) {
            if(yyjson_is_obj(element)) {
                read_light(element, group, out);
            }
        }
    }

    // Loose shared values: anything more than one layer has to agree about that is not a light.
    // Same path grammar, same panel, same settings file - a light is simply the structured case.
    yyjson_val* values = yyjson_obj_get(shared, "values");
    if(values == nullptr || !yyjson_is_arr(values)) {
        return;
    }

    std::size_t index = 0;
    std::size_t max = 0;
    yyjson_val* element = nullptr;
    yyjson_arr_foreach(values, index, max, element) {
        if(!yyjson_is_obj(element)) {
            continue;
        }

        tw::skybox::package::param value = read_param(element, "Shared");
        if(value.id.empty()) {
            out.diagnostics.emplace_back("a shared value with no id was ignored - layers bind to values by id");
            continue;
        }

        out.values.push_back(std::move(value));
    }
}

void read_bindings(yyjson_val* object, tw::skybox::package::layer& out)
{
    yyjson_val* bind = yyjson_obj_get(object, "bind");
    if(bind == nullptr || !yyjson_is_obj(bind)) {
        return;
    }

    yyjson_obj_iter iter;
    yyjson_obj_iter_init(bind, &iter);

    while(yyjson_val* key = yyjson_obj_iter_next(&iter)) {
        yyjson_val* value = yyjson_obj_iter_get_val(key);

        tw::skybox::package::binding entry;
        entry.variable.assign(as_string(key));
        entry.source.assign(as_string(value));

        if(!entry.variable.empty() && !entry.source.empty()) {
            out.bindings.push_back(std::move(entry));
        }
    }
}

// Parameters arrive grouped: a list of groups, each with a heading and its own list.
//
// The old annotation block made `group` a mode switch - a line that changed the meaning of every
// line after it - and the first cut of this manifest carried that over as a "group" key appearing
// on some entries and not others. That is a stateful parse dressed as data: what an entry means
// depends on what stands above it. Nesting says the same thing structurally, which also gives a
// future editor a node to attach a group to instead of a convention to preserve.
//
// A group may omit its name, and then its parameters draw without a heading. One rule rather than a
// second top-level list for the ungrouped ones.
void read_params(yyjson_val* object, tw::skybox::package::layer& out, tw::skybox::package::manifest& sky)
{
    yyjson_val* groups = yyjson_obj_get(object, "params");
    if(groups == nullptr || !yyjson_is_arr(groups)) {
        return;
    }

    std::size_t group_index = 0;
    std::size_t group_max = 0;
    yyjson_val* group_element = nullptr;
    yyjson_arr_foreach(groups, group_index, group_max, group_element) {
        if(!yyjson_is_obj(group_element)) {
            continue;
        }

        const std::string_view group = member_string(group_element, "group");

        yyjson_val* entries = yyjson_obj_get(group_element, "params");
        if(entries == nullptr || !yyjson_is_arr(entries)) {
            continue;
        }

        std::size_t index = 0;
        std::size_t max = 0;
        yyjson_val* element = nullptr;
        yyjson_arr_foreach(entries, index, max, element) {
            if(!yyjson_is_obj(element)) {
                continue;
            }

            tw::skybox::package::param param = read_param(element, group);

            // One or the other, never both and never neither: a knob that names nothing has nowhere
            // to put what the user moves it to.
            if(param.variable.empty() == param.property.empty()) {
                sky.diagnostics.emplace_back("layer '" + out.id + "': knob '" + param.label
                    + "' must name exactly one of \"var\" (a shader variable) or \"prop\" (a layer property)");
                continue;
            }

            out.params.push_back(std::move(param));
        }
    }
}

void read_layers(yyjson_val* root, tw::skybox::package::manifest& out)
{
    yyjson_val* layers = yyjson_obj_get(root, "layers");
    if(layers == nullptr || !yyjson_is_arr(layers)) {
        out.diagnostics.emplace_back("no 'layers' array - a sky with no layers draws nothing");
        return;
    }

    std::size_t index = 0;
    std::size_t max = 0;
    yyjson_val* element = nullptr;
    yyjson_arr_foreach(layers, index, max, element) {
        if(!yyjson_is_obj(element)) {
            continue;
        }

        tw::skybox::package::layer layer;
        layer.id.assign(member_string(element, "id"));
        layer.shader.assign(member_string(element, "shader"));
        layer.enabled = as_bool(yyjson_obj_get(element, "enabled"), true);

        const std::string_view kind_text = member_string(element, "kind");
        layer.kind = parse_kind(kind_text);

        if(layer.id.empty()) {
            out.diagnostics.emplace_back("a layer with no id was ignored");
            continue;
        }

        if(layer.kind == tw::skybox::package::layer_kind::unknown) {
            out.diagnostics.emplace_back("layer '" + layer.id + "': unknown kind '" + std::string(kind_text)
                + "' - this build understands fullsky and sprites");
            continue;
        }

        read_bindings(element, layer);
        read_params(element, layer, out);

        out.layers.push_back(std::move(layer));
    }
}

// Every file the manifest names, so the reload watch has something to stat. The manifest itself is
// always in the list; the shaders are what actually get edited.
void collect_watched(tw::skybox::package::manifest& out)
{
    out.watched.push_back(out.root / k_manifest_name);

    for(const tw::skybox::package::layer& layer : out.layers) {
        if(layer.shader.empty()) {
            continue;
        }

        std::error_code ec;

        // A `sprites` layer names the stem of a .vs/.ps pair, and a `fullsky` layer names a file.
        // Both are covered by trying the plain path first and the two suffixes after, which also
        // means a package that ships only one half of a pair still watches the half it has.
        for(const std::string_view suffix : { std::string_view {}, std::string_view { ".vs.hlsl" }, std::string_view { ".ps.hlsl" } }) {
            std::filesystem::path candidate = out.root / (layer.shader + std::string(suffix));
            if(std::filesystem::is_regular_file(candidate, ec)) {
                out.watched.push_back(std::move(candidate));
            }
        }
    }
}

bool read_file(const std::filesystem::path& path, std::string& out)
{
    std::ifstream file { path, std::ios::binary };
    if(!file.is_open()) {
        return false;
    }

    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    if(size < 0) {
        return false;
    }
    file.seekg(0, std::ios::beg);

    out.resize(static_cast<std::size_t>(size));
    if(!out.empty()) {
        file.read(out.data(), size);
    }

    return true;
}
} // namespace

namespace tw::skybox::package
{
manifest load_directory(const std::filesystem::path& root)
{
    manifest out;
    out.root = root;

    const std::filesystem::path manifest_path = root / k_manifest_name;

    std::string text;
    if(!read_file(manifest_path, text)) {
        out.diagnostics.emplace_back("could not read " + manifest_path.filename().string());
        return out;
    }

    document parsed;
    yyjson_read_err error {};
    parsed.doc = yyjson_read_opts(text.data(), text.size(), k_read_flags, nullptr, &error);

    if(parsed.doc == nullptr) {
        // The offset is worth carrying through: "invalid character" without a position is a worse
        // message than no message, on a file that may be a thousand lines long.
        out.diagnostics.emplace_back(
            std::string { k_manifest_name } + ": " + (error.msg != nullptr ? error.msg : "could not be parsed") + " at byte "
            + std::to_string(error.pos));
        return out;
    }

    yyjson_val* json_root = yyjson_doc_get_root(parsed.doc);
    if(json_root == nullptr || !yyjson_is_obj(json_root)) {
        out.diagnostics.emplace_back(std::string { k_manifest_name } + ": the top level must be an object");
        return out;
    }

    out.format = as_int(yyjson_obj_get(json_root, "format"), 0);
    out.name.assign(member_string(json_root, "name"));
    out.author.assign(member_string(json_root, "author"));
    out.version = as_int(yyjson_obj_get(json_root, "version"), 0);

    if(out.format > k_supported_format) {
        // Read it anyway. A newer format is far more likely to have added keys - which are ignored
        // harmlessly - than to have changed the meaning of one, and refusing outright would turn a
        // cosmetic version bump into a sky that does not load.
        out.diagnostics.emplace_back("format " + std::to_string(out.format) + " is newer than this build understands ("
            + std::to_string(k_supported_format) + ") - reading it anyway, some of it may be ignored");
    }

    if(yyjson_val* shared = yyjson_obj_get(json_root, "shared"); shared != nullptr && yyjson_is_obj(shared)) {
        read_shared(shared, out);
    }

    read_layers(json_root, out);
    collect_watched(out);

    if(out.name.empty()) {
        out.name = root.stem().string();
    }

    TW_LOG_INFO("sky_package: '{}' - {} layer(s), {} light(s), {} shared value(s), {} diagnostic(s)",
        out.name,
        out.layers.size(),
        out.lights.size(),
        out.values.size(),
        out.diagnostics.size());

    for(const std::string& line : out.diagnostics) {
        TW_LOG_WARNING("sky_package: {}", line);
    }

    return out;
}

std::filesystem::file_time_type newest_write_time(const manifest& package) noexcept
{
    std::filesystem::file_time_type newest {};

    for(const std::filesystem::path& path : package.watched) {
        std::error_code ec;
        const std::filesystem::file_time_type stamp = std::filesystem::last_write_time(path, ec);
        if(!ec && stamp > newest) {
            newest = stamp;
        }
    }

    return newest;
}

std::string_view kind_name(layer_kind kind) noexcept
{
    switch(kind) {
        case layer_kind::fullsky:
            return "fullsky";
        case layer_kind::sprites:
            return "sprites";
        default:
            return "unknown";
    }
}
} // namespace tw::skybox::package
