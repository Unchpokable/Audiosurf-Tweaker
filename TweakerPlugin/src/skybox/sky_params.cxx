#include "pch.hxx"

#include "skybox/sky_params.hxx"

#include "plugin/diagnostics.hxx"

namespace
{
constexpr std::string_view k_marker = "@sky";
constexpr std::string_view k_comment_end = "*/";
constexpr std::string_view k_components = "xyzw";

std::string_view trim(std::string_view s) noexcept
{
    while(!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r' || s.front() == '\n')) {
        s.remove_prefix(1);
    }
    while(!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) {
        s.remove_suffix(1);
    }
    return s;
}

float parse_float(std::string_view s, float fallback) noexcept
{
    const std::string_view value = trim(s);

    float out = fallback;
    std::from_chars(value.data(), value.data() + value.size(), out);
    return out;
}

std::vector<std::string_view> split(std::string_view text, char separator)
{
    std::vector<std::string_view> parts;

    std::size_t start = 0;
    while(start <= text.size()) {
        const std::size_t at = text.find(separator, start);
        if(at == std::string_view::npos) {
            parts.push_back(trim(text.substr(start)));
            break;
        }

        parts.push_back(trim(text.substr(start, at - start)));
        start = at + 1;
    }

    return parts;
}

// The block runs from the marker to the end of the comment it sits in. Deliberately not a search
// for a matching opener: the marker is what identifies the block, and a shader that contains the
// word "@sky" outside a comment has bigger problems than this.
std::string_view extract_block(std::string_view source)
{
    const std::size_t marker = source.find(k_marker);
    if(marker == std::string_view::npos) {
        return {};
    }

    const std::size_t start = marker + k_marker.size();
    const std::size_t end = source.find(k_comment_end, start);

    return source.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
}

// "g_eclipse.xy" -> the variable and the components it names. An absent swizzle means .x for a
// scalar; a colour ignores it and always takes xyz.
void split_reference(std::string_view reference, std::string_view& out_variable, std::string_view& out_swizzle)
{
    const std::size_t dot = reference.find('.');
    if(dot == std::string_view::npos) {
        out_variable = reference;
        out_swizzle = {};
        return;
    }

    out_variable = reference.substr(0, dot);
    out_swizzle = reference.substr(dot + 1);
}

const tw::skybox::bytecode::constant* find_constant(const tw::skybox::bytecode::reflection& reflection, std::string_view name)
{
    for(const auto& constant : reflection.floats) {
        if(constant.name == name) {
            return &constant;
        }
    }

    return nullptr;
}

int component_index(std::string_view swizzle) noexcept
{
    if(swizzle.empty()) {
        return 0;
    }

    const std::size_t at = k_components.find(swizzle.front());

    return at == std::string_view::npos ? -1 : static_cast<int>(at);
}

std::string make_key(int reg, int component, int count)
{
    std::string key = "c" + std::to_string(reg);
    for(int i = 0; i < count; ++i) {
        key += k_components[static_cast<std::size_t>(component + i)];
    }

    return key;
}

// Shared by both line kinds: resolve the variable, work out the first component, and refuse
// anything that would run off the end of a float4.
bool resolve(const tw::skybox::bytecode::reflection& reflection,
    std::string_view reference,
    int count,
    bool warn_unresolved,
    tw::skybox::sky_param& out)
{
    std::string_view variable;
    std::string_view swizzle;
    split_reference(reference, variable, swizzle);

    const auto* constant = find_constant(reflection, variable);
    if(constant == nullptr) {
        if(warn_unresolved) {
            TW_LOG_WARNING("sky_params: '{}' is not a constant this shader declares - parameter ignored", variable);
        }
        return false;
    }

    const int component = count == 3 ? 0 : component_index(swizzle);
    if(component < 0 || component + count > 4) {
        if(warn_unresolved) {
            TW_LOG_WARNING("sky_params: '{}' does not name {} usable component(s) - parameter ignored", reference, count);
        }
        return false;
    }

    out.reg = constant->reg;
    out.component = component;
    out.count = count;
    out.key = make_key(out.reg, component, count);
    out.widget_id = "##" + out.key;

    return true;
}

tw::skybox::param_block parse_block(std::string_view source, const tw::skybox::bytecode::reflection& reflection, bool warn_unresolved)
{
    using tw::skybox::param_block;
    using tw::skybox::sky_param;

    param_block block;

    const std::string_view text = extract_block(source);
    if(text.empty()) {
        return block;
    }

    std::string_view group;

    for(const std::string_view raw_line : split(text, '\n')) {
        const std::string_view line = trim(raw_line);
        if(line.empty() || line.starts_with("//")) {
            continue;
        }

        const std::size_t eq = line.find('=');
        if(eq == std::string_view::npos) {
            continue;
        }

        const std::string_view key = trim(line.substr(0, eq));
        const std::vector<std::string_view> fields = split(line.substr(eq + 1), '|');

        if(key == "name") {
            block.display_name.assign(trim(line.substr(eq + 1)));
            continue;
        }

        if(key == "version") {
            const std::string_view value = trim(line.substr(eq + 1));
            std::from_chars(value.data(), value.data() + value.size(), block.version);
            continue;
        }

        // Applies from here to the next `group` line. Set even when the parameters that follow do
        // not resolve, so a later one in the same group still lands under the right heading.
        if(key == "group") {
            group = trim(line.substr(eq + 1));
            continue;
        }

        if(key == "param" && fields.size() >= 3) {
            sky_param param;
            if(!resolve(reflection, fields[0], 1, warn_unresolved, param)) {
                continue;
            }

            param.group.assign(group);
            param.label.assign(fields[1]);
            param.value[0] = parse_float(fields[2], 0.f);
            param.min_value = fields.size() > 3 ? parse_float(fields[3], 0.f) : 0.f;
            param.max_value = fields.size() > 4 ? parse_float(fields[4], 1.f) : 1.f;

            if(param.max_value <= param.min_value) {
                param.max_value = param.min_value + 1.f;
            }

            block.params.push_back(std::move(param));
            continue;
        }

        if(key == "color" && fields.size() >= 3) {
            sky_param param;
            if(!resolve(reflection, fields[0], 3, warn_unresolved, param)) {
                continue;
            }

            param.group.assign(group);
            param.label.assign(fields[1]);

            const std::vector<std::string_view> channels = split(fields[2], ',');
            for(std::size_t i = 0; i < param.value.size(); ++i) {
                param.value[i] = i < channels.size() ? parse_float(channels[i], 0.f) : 0.f;
            }

            block.params.push_back(std::move(param));
        }
    }

    for(sky_param& param : block.params) {
        param.default_value = param.value;
    }

    return block;
}
} // namespace

namespace tw::skybox
{
param_block parse_params(std::string_view source, const bytecode::reflection& reflection)
{
    param_block block = parse_block(source, reflection, true);

    if(!block.params.empty()) {
        TW_LOG_INFO("sky_params: {} parameter(s) declared by the program", block.params.size());
    }

    return block;
}

param_block parse_shared_params(std::string_view source, const bytecode::reflection& reflection)
{
    param_block block = parse_block(source, reflection, false);

    if(!block.params.empty()) {
        TW_LOG_INFO("sky_params: {} parameter(s) taken from the shared header", block.params.size());
    }

    return block;
}

void order_by_group(std::vector<sky_param>& params)
{
    std::vector<sky_param> ordered;
    ordered.reserve(params.size());

    // One pass per distinct group rather than a sort: the comparison a sort needs is "which group
    // came first", which is itself a search through the list, and the list is thirty items long.
    for(std::size_t i = 0; i < params.size(); ++i) {
        const bool seen = std::any_of(params.begin(), params.begin() + static_cast<std::ptrdiff_t>(i), [&](const sky_param& earlier) {
            return earlier.group == params[i].group;
        });

        if(seen) {
            continue;
        }

        for(sky_param& param : params) {
            if(param.group == params[i].group) {
                ordered.push_back(param);
            }
        }
    }

    params = std::move(ordered);
}

void apply_params(std::span<const sky_param> params, std::span<float> constants) noexcept
{
    for(const sky_param& param : params) {
        for(int i = 0; i < param.count; ++i) {
            const auto slot = static_cast<std::size_t>(param.reg * 4 + param.component + i);
            if(slot < constants.size()) {
                constants[slot] = param.value[static_cast<std::size_t>(i)];
            }
        }
    }
}
} // namespace tw::skybox
