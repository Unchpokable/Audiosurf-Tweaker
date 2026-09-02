#include "pch.hxx"

#include "skybox/sky_settings.hxx"

#include "plugin/diagnostics.hxx"

#include "skybox/sky_paths.hxx"

#include <libyyjson/yyjson.h>

namespace
{
constexpr yyjson_read_flag k_read_flags = YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS;

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

// Trailing zeros are noise in a file people are meant to read and hand-edit, and a fixed precision
// turns 0.075 into 0.075000. Shortest round-trip is what std::to_chars gives.
std::string number_text(float value)
{
    std::array<char, 32> buffer {};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    if(result.ec != std::errc {}) {
        return "0";
    }

    return std::string { buffer.data(), result.ptr };
}

std::string value_text(const tw::skybox::settings::entry& entry)
{
    if(entry.count < 3) {
        return number_text(entry.value[0]);
    }

    return "[" + number_text(entry.value[0]) + ", " + number_text(entry.value[1]) + ", " + number_text(entry.value[2]) + "]";
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

namespace tw::skybox::settings
{
void store::load(const std::filesystem::path& path)
{
    m_path = path;
    m_entries.clear();

    std::string text;
    if(!read_file(path, text)) {
        return;
    }

    document parsed;
    yyjson_read_err error {};
    parsed.doc = yyjson_read_opts(text.data(), text.size(), k_read_flags, nullptr, &error);

    if(parsed.doc == nullptr) {
        TW_LOG_WARNING("sky_settings: '{}': {} at byte {} - starting from the sky's own defaults",
            path.filename().string(),
            error.msg != nullptr ? error.msg : "could not be parsed",
            error.pos);
        return;
    }

    yyjson_val* root = yyjson_doc_get_root(parsed.doc);
    if(root == nullptr || !yyjson_is_obj(root)) {
        return;
    }

    yyjson_val* layers = yyjson_obj_get(root, "layers");
    if(layers == nullptr || !yyjson_is_obj(layers)) {
        return;
    }

    yyjson_obj_iter layer_iter;
    yyjson_obj_iter_init(layers, &layer_iter);

    while(yyjson_val* layer_key = yyjson_obj_iter_next(&layer_iter)) {
        yyjson_val* params = yyjson_obj_iter_get_val(layer_key);
        if(!yyjson_is_obj(params)) {
            continue;
        }

        const std::string_view layer_name { yyjson_get_str(layer_key), yyjson_get_len(layer_key) };

        yyjson_obj_iter param_iter;
        yyjson_obj_iter_init(params, &param_iter);

        while(yyjson_val* param_key = yyjson_obj_iter_next(&param_iter)) {
            yyjson_val* value = yyjson_obj_iter_get_val(param_key);

            entry item;
            item.layer.assign(layer_name);
            item.id.assign(yyjson_get_str(param_key), yyjson_get_len(param_key));

            if(yyjson_is_num(value)) {
                item.count = 1;
                item.value[0] = static_cast<float>(yyjson_get_num(value));
            }
            else if(yyjson_is_arr(value)) {
                item.count = 3;

                std::size_t index = 0;
                std::size_t max = 0;
                yyjson_val* element = nullptr;
                yyjson_arr_foreach(value, index, max, element) {
                    if(index < item.value.size() && yyjson_is_num(element)) {
                        item.value[index] = static_cast<float>(yyjson_get_num(element));
                    }
                }
            }
            else {
                continue;
            }

            m_entries.push_back(std::move(item));
        }
    }

    TW_LOG_INFO("sky_settings: '{}' - {} override(s)", path.filename().string(), m_entries.size());
}

bool store::lookup(std::string_view layer, std::string_view id, std::array<float, 3>& out) const noexcept
{
    for(const entry& item : m_entries) {
        if(item.layer == layer && item.id == id) {
            out = item.value;
            return true;
        }
    }

    return false;
}

void store::assign(std::string_view layer, std::string_view id, const std::array<float, 3>& value, int count)
{
    for(entry& item : m_entries) {
        if(item.layer == layer && item.id == id) {
            item.value = value;
            item.count = count;
            return;
        }
    }

    entry item;
    item.layer.assign(layer);
    item.id.assign(id);
    item.value = value;
    item.count = count;

    m_entries.push_back(std::move(item));
}

void store::save(std::string_view display_name) const
{
    if(m_path.empty()) {
        return;
    }

    std::error_code ec;
    std::filesystem::create_directories(m_path.parent_path(), ec);

    std::ofstream file { m_path, std::ios::trunc };
    if(!file.is_open()) {
        TW_LOG_ERROR("sky_settings: cannot write '{}'", m_path.string());
        return;
    }

    file << "{\n";
    file << "  // Overrides for \"" << display_name << "\". Everything not listed uses the sky's own default.\n";
    file << "  // Delete a line to go back to that default, or delete the file to reset the sky.\n";
    file << "  \"sky\": \"" << display_name << "\",\n";
    file << "  \"layers\": {\n";

    // One section, because a sky without a manifest is a sky of one layer that does not say so - the
    // same shape everything above this file already works in.
    std::string layer = "sky";
    if(!m_entries.empty()) {
        layer = m_entries.front().layer;
    }

    file << "    \"" << layer << "\": {\n";

    for(std::size_t i = 0; i < m_entries.size(); ++i) {
        if(i > 0) {
            file << ",\n";
        }

        file << "      \"" << m_entries[i].id << "\": " << value_text(m_entries[i]);
    }

    file << "\n    }\n  }\n}\n";
}

void store::save(const package::manifest& sky) const
{
    if(m_path.empty()) {
        return;
    }

    std::error_code ec;
    std::filesystem::create_directories(m_path.parent_path(), ec);

    std::ofstream file { m_path, std::ios::trunc };
    if(!file.is_open()) {
        TW_LOG_ERROR("sky_settings: cannot write '{}'", m_path.string());
        return;
    }

    // Written rather than remembered, so a hand-edited file that loses the header still says what
    // it belongs to next time somebody opens it.
    file << "{\n";
    file << "  // Overrides for \"" << sky.name << "\". Everything not listed uses the sky's own default.\n";
    file << "  // Delete a line to go back to that default, or delete the file to reset the sky.\n";
    file << "  \"sky\": \"" << sky.name << "\",\n";
    file << "  \"layers\": {\n";

    // Planned before anything is written, because getting commas and headings right while also
    // deciding what goes where is how the first cut of this came out with a stray blank line and a
    // misplaced separator. One pass decides the shape, the next prints it.
    struct section {
        std::string layer;
        bool known {}; // in the manifest, so its entries carry group headings and manifest order
        std::vector<std::size_t> entries;
    };

    std::vector<section> sections;
    std::vector<bool> taken(m_entries.size(), false);

    // The sky's own block first, in the order the manifest declares its lights and values.
    //
    // It is not a layer and never was - it is what the layers bind to - but it is stored under a
    // layer id of its own so that moving a light between layers cannot lose its value, because it
    // never belonged to one. Ordered here rather than left to the "no longer in the manifest"
    // bucket below, which would file the sky's own lights under "kept so they are not lost".
    {
        section shared;
        shared.layer = "shared";
        shared.known = true;

        const auto take = [&](const std::string& key) {
            for(std::size_t i = 0; i < m_entries.size(); ++i) {
                if(!taken[i] && m_entries[i].layer == "shared" && m_entries[i].id == key) {
                    taken[i] = true;
                    shared.entries.push_back(i);
                }
            }
        };

        for(const package::light& light : sky.lights) {
            for(const std::string_view field : { "bearing", "elevation", "color", "intensity" }) {
                take("lights." + light.id + "." + std::string { field });
            }
        }

        for(const package::param& value : sky.values) {
            take("values." + value.id);
        }

        if(!shared.entries.empty()) {
            sections.push_back(std::move(shared));
        }
    }

    for(const package::layer& layer : sky.layers) {
        section current;
        current.layer = layer.id;
        current.known = true;

        for(const package::param& param : layer.params) {
            for(std::size_t i = 0; i < m_entries.size(); ++i) {
                if(!taken[i] && m_entries[i].layer == layer.id && m_entries[i].id == param.id) {
                    taken[i] = true;
                    current.entries.push_back(i);
                }
            }
        }

        if(!current.entries.empty()) {
            sections.push_back(std::move(current));
        }
    }

    // Anything the manifest no longer mentions, grouped by its layer in the order first seen. Kept
    // on purpose: a layer switched off for an evening must not lose its tuning, and this is the one
    // thing the old flat file was actually right about.
    for(std::size_t i = 0; i < m_entries.size(); ++i) {
        if(taken[i]) {
            continue;
        }

        // Matched against *every* section, not only the unknown ones. A layer the manifest still
        // has can perfectly well hold a value for a knob it no longer declares - remove one group
        // from a shader and that is exactly what happens - and filing those under a second section
        // with the same name emits the layer twice. Two identical keys in one JSON object is not an
        // error the reader reports; it silently keeps one of them, so half the sky's settings would
        // vanish on the next load.
        auto found = std::find_if(sections.begin(), sections.end(), [&](const section& s) {
            return s.layer == m_entries[i].layer;
        });

        if(found == sections.end()) {
            section current;
            current.layer = m_entries[i].layer;
            current.known = false;
            sections.push_back(std::move(current));
            found = sections.end() - 1;
        }

        found->entries.push_back(i);
        taken[i] = true;
    }

    // The group each parameter belongs to, for the comments. Built once rather than searched per
    // entry, and only for layers the manifest still describes.
    const auto group_of = [&sky](const std::string& layer_id, const std::string& param_id) -> std::string {
        for(const package::layer& layer : sky.layers) {
            if(layer.id != layer_id) {
                continue;
            }
            for(const package::param& param : layer.params) {
                if(param.id == param_id) {
                    return param.group;
                }
            }
        }
        return {};
    };

    for(std::size_t s = 0; s < sections.size(); ++s) {
        const section& current = sections[s];

        if(s > 0) {
            file << ",\n";
        }

        if(!current.known) {
            file << "    // not in the sky's manifest right now; kept so it is not lost\n";
        }

        file << "    \"" << current.layer << "\": {\n";

        std::string heading;

        for(std::size_t e = 0; e < current.entries.size(); ++e) {
            const entry& item = m_entries[current.entries[e]];

            if(e > 0) {
                file << ",\n";
            }

            if(current.known) {
                if(const std::string group = group_of(current.layer, item.id); group != heading) {
                    heading = group;
                    if(!heading.empty()) {
                        file << (e > 0 ? "\n" : "") << "      // " << heading << "\n";
                    }
                }
            }

            file << "      \"" << item.id << "\": " << value_text(item);
        }

        file << "\n    }";
    }

    file << "\n  }\n}\n";
}

std::filesystem::path path_for(std::string_view package_stem)
{
    const std::filesystem::path root = tw::skybox::dll_directory();
    if(root.empty()) {
        return {};
    }

    // Sanitised rather than trusted: the stem comes from a directory name, and a sky called
    // "../../autoexec" has no business deciding where this file lands.
    std::string safe;
    safe.reserve(package_stem.size());

    for(const char c : package_stem) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == ' '
            || c == '.';
        safe += ok ? c : '_';
    }

    while(!safe.empty() && (safe.front() == '.' || safe.front() == ' ')) {
        safe.erase(safe.begin());
    }
    while(!safe.empty() && (safe.back() == '.' || safe.back() == ' ')) {
        safe.pop_back();
    }

    if(safe.empty()) {
        safe = "sky";
    }

    return root / "Skies" / (safe + ".json");
}
} // namespace tw::skybox::settings
