#include "pch.hxx"

#include "skybox/sky_shared.hxx"

#include "plugin/diagnostics.hxx"

namespace
{
constexpr float k_to_radians = 3.14159265358979323846f / 180.f;

// How far a `relative_to` or `same_as` chain is followed before giving up. Four is far more than any
// real sky needs and the point is not the depth - it is that a manifest saying "a is relative to b"
// and "b is relative to a" must not hang the game.
constexpr int k_max_relation_depth = 4;

constexpr std::string_view k_lights_prefix = "lights.";
constexpr std::string_view k_suns_prefix = "suns.";
constexpr std::string_view k_values_prefix = "values.";

// "lights.<id>.<field>" without building the string to compare against. Called from resolve, which
// is a cold path but is also noexcept, and an allocation there would be a way to terminate the
// process over a typo in somebody's manifest.
bool matches_light_field(std::string_view key, std::string_view id, std::string_view field) noexcept
{
    if(!key.starts_with(k_lights_prefix)) {
        return false;
    }
    key.remove_prefix(k_lights_prefix.size());

    if(!key.starts_with(id)) {
        return false;
    }
    key.remove_prefix(id.size());

    if(key.empty() || key.front() != '.') {
        return false;
    }
    key.remove_prefix(1);

    return key == field;
}

std::string light_key(std::string_view id, std::string_view field)
{
    std::string key { k_lights_prefix };
    key.append(id);
    key.push_back('.');
    key.append(field);

    return key;
}

// A field the author left out still has an answer, and the answer is the neutral one: an undeclared
// colour is white and an undeclared intensity is one. Returning "no such value" instead would mean
// a cloud layer binding a colour goes black the moment a sky does not bother to tint its sun, which
// is the opposite of what leaving it out means.
bool neutral_for(std::string_view field, tw::skybox::shared::value& out) noexcept
{
    if(field == "color") {
        out = { { 1.f, 1.f, 1.f, 0.f }, 3 };
        return true;
    }

    if(field == "intensity") {
        out = { { 1.f, 0.f, 0.f, 0.f }, 1 };
        return true;
    }

    if(field == "bearing" || field == "elevation") {
        out = { {}, 1 };
        return true;
    }

    return false;
}
} // namespace

namespace tw::skybox::shared
{
void state::adopt(std::shared_ptr<const package::manifest> sky)
{
    // Kept, not cleared. Editing Config.json while the game runs is the loop this format was built
    // around, and a reload that reset every knob to its default would make saving the file a
    // punishment for having tuned anything.
    std::vector<knob> previous;
    previous.swap(m_knobs);

    m_sky = std::move(sky);
    if(m_sky == nullptr) {
        return;
    }

    const auto carry_over = [&previous](knob& entry) {
        for(const knob& old : previous) {
            if(old.key == entry.key && old.count == entry.count) {
                entry.value = old.value;
                return;
            }
        }
    };

    const auto add = [&](std::string key, const package::param& source, std::string_view fallback_label) {
        if(!source.declared) {
            return;
        }

        knob entry;
        entry.key = std::move(key);
        entry.label = source.label.empty() ? std::string { fallback_label } : source.label;
        entry.group = source.group;
        entry.count = source.count;
        entry.min_value = source.min_value;
        entry.max_value = source.max_value;
        entry.value = source.value;
        entry.default_value = source.default_value;

        carry_over(entry);

        m_knobs.push_back(std::move(entry));
    };

    for(const package::light& light : m_sky->lights) {
        add(light_key(light.id, "bearing"), light.bearing, "Bearing (deg)");
        add(light_key(light.id, "elevation"), light.elevation, "Elevation (deg)");
        add(light_key(light.id, "color"), light.color, "Colour");
        add(light_key(light.id, "intensity"), light.intensity, "Intensity");
    }

    for(const package::param& entry : m_sky->values) {
        add(std::string { k_values_prefix } + entry.id, entry, entry.id);
    }

    TW_LOG_INFO("sky_shared: '{}' - {} light(s), {} shared knob(s)", m_sky->name, m_sky->lights.size(), m_knobs.size());
}

const knob* state::find(std::string_view key) const noexcept
{
    for(const knob& entry : m_knobs) {
        if(entry.key == key) {
            return &entry;
        }
    }

    return nullptr;
}

bool state::read(std::string_view key, std::array<float, 3>& out) const noexcept
{
    const knob* entry = find(key);
    if(entry == nullptr) {
        return false;
    }

    out = entry->value;

    return true;
}

bool state::write(std::string_view key, std::span<const float> in) noexcept
{
    for(knob& entry : m_knobs) {
        if(entry.key != key) {
            continue;
        }

        for(int i = 0; i < entry.count && i < static_cast<int>(in.size()); ++i) {
            entry.value[static_cast<std::size_t>(i)] = in[static_cast<std::size_t>(i)];
        }

        return true;
    }

    return false;
}

void state::reset() noexcept
{
    for(knob& entry : m_knobs) {
        entry.value = entry.default_value;
    }
}

float state::absolute_bearing(const package::light& light, int depth) const noexcept
{
    std::array<float, 3> own {};
    float bearing = 0.f;

    for(const knob& entry : m_knobs) {
        if(matches_light_field(entry.key, light.id, "bearing")) {
            own = entry.value;
            bearing = own[0];
            break;
        }
    }

    if(light.bearing_relative_to.empty() || depth >= k_max_relation_depth) {
        return bearing;
    }

    const package::light* base = m_sky->find_light(light.bearing_relative_to);

    return base == nullptr ? bearing : bearing + absolute_bearing(*base, depth + 1);
}

float state::effective_elevation(const package::light& light, int depth) const noexcept
{
    for(const knob& entry : m_knobs) {
        if(matches_light_field(entry.key, light.id, "elevation")) {
            return entry.value[0];
        }
    }

    if(light.elevation_same_as.empty() || depth >= k_max_relation_depth) {
        return 0.f;
    }

    const package::light* base = m_sky->find_light(light.elevation_same_as);

    return base == nullptr ? 0.f : effective_elevation(*base, depth + 1);
}

bool state::resolve(std::string_view path, value& out) const noexcept
{
    if(m_sky == nullptr) {
        return false;
    }

    if(path.starts_with(k_values_prefix)) {
        const knob* entry = find(path);
        if(entry == nullptr) {
            return false;
        }

        out.count = entry->count;
        for(int i = 0; i < entry->count; ++i) {
            out.data[static_cast<std::size_t>(i)] = entry->value[static_cast<std::size_t>(i)];
        }

        return true;
    }

    std::string_view rest;
    if(path.starts_with(k_lights_prefix)) {
        rest = path.substr(k_lights_prefix.size());
    }
    else if(path.starts_with(k_suns_prefix)) {
        rest = path.substr(k_suns_prefix.size());
    }
    else {
        return false;
    }

    const std::size_t dot = rest.find('.');
    if(dot == std::string_view::npos) {
        return false;
    }

    const std::string_view id = rest.substr(0, dot);
    std::string_view field = rest.substr(dot + 1);

    if(field == "strength") {
        field = "intensity";
    }

    const package::light* light = m_sky->find_light(id);
    if(light == nullptr) {
        return false;
    }

    // The derived forms first, because they are the ones with no stored knob behind them and the
    // ones a lighting shader actually asks for.
    if(field == "direction") {
        if(light->kind != package::light_kind::directional) {
            return false;
        }

        const float bearing = absolute_bearing(*light, 0) * k_to_radians;
        const float elevation = effective_elevation(*light, 0) * k_to_radians;
        const float horizontal = std::cos(elevation);

        out.data = { std::sin(bearing) * horizontal, std::sin(elevation), std::cos(bearing) * horizontal, 0.f };
        out.count = 3;

        return true;
    }

    if(field == "radiance") {
        value color {};
        value intensity {};

        if(!resolve_field(*light, "color", color) || !resolve_field(*light, "intensity", intensity)) {
            return false;
        }

        for(std::size_t i = 0; i < 3; ++i) {
            out.data[i] = color.data[i] * intensity.data[0];
        }
        out.count = 3;

        return true;
    }

    // An elevation borrowed through `same_as` has no knob of its own, so it goes through the chain
    // rather than through the lookup.
    if(field == "elevation") {
        out.data = { effective_elevation(*light, 0), 0.f, 0.f, 0.f };
        out.count = 1;

        return true;
    }

    return resolve_field(*light, field, out);
}

bool state::resolve_field(const package::light& light, std::string_view field, value& out) const noexcept
{
    for(const knob& entry : m_knobs) {
        if(!matches_light_field(entry.key, light.id, field)) {
            continue;
        }

        out.count = entry.count;
        for(int i = 0; i < entry.count; ++i) {
            out.data[static_cast<std::size_t>(i)] = entry.value[static_cast<std::size_t>(i)];
        }

        return true;
    }

    return neutral_for(field, out);
}

void apply_bindings(const state& values,
    std::span<const resolved_binding> bindings,
    std::span<float> constants,
    std::span<float> vertex_constants) noexcept
{
    for(const resolved_binding& binding : bindings) {
        if(!binding.valid()) {
            continue;
        }

        value source {};
        if(!values.resolve(binding.source, source)) {
            continue;
        }

        // The target's swizzle wins when it has one, so binding a three-component radiance into
        // `g_tint.x` writes one number rather than trampling the two beside it. With no swizzle the
        // source decides its own width.
        const int width = binding.count > 0 ? (std::min)(binding.count, source.count) : source.count;
        const std::span<float> block = binding.vertex_stage ? vertex_constants : constants;

        for(int i = 0; i < width && binding.component + i < 4; ++i) {
            const auto slot = static_cast<std::size_t>(binding.reg * 4 + binding.component + i);
            if(slot < block.size()) {
                block[slot] = source.data[static_cast<std::size_t>(i)];
            }
        }
    }
}
} // namespace tw::skybox::shared
