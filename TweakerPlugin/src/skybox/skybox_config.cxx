#include "pch.hxx"

#include "skybox/skybox_config.hxx"

#include "plugin/diagnostics.hxx"
#include "plugin/paths.hxx"

#include <libyyjson/yyjson.h>

namespace
{
using tw::skybox::config::k_default_packed;

// Comments and trailing commas are accepted on read because people edit this file by hand, the same as
// the per-sky files. They are not preserved on write: the file is regenerated from the values it holds.
constexpr yyjson_read_flag k_read_flags = YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS;

struct values {
    bool enabled = true;
    tw::skybox::config::selection sky { tw::skybox::entry_kind::packed, std::string { k_default_packed } };
    float hdr_exposure = 1.f;
    int min_face_size = 0;
    float yaw_degrees = 0.f;
    float pitch_degrees = 0.f;
    bool z_up = false;
    int shader_quality = 100;
};

values g_values;
std::filesystem::path g_path;

// Set when the file exists but could not be read as a settings object. While it is set, nothing is
// written: the user's file is broken, not absent, and the next save would replace it wholesale.
// Cleared by the first explicit change, which is the user telling us the in-memory state is what they want.
bool g_hold_writes = false;

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

bool read_bool(yyjson_val* object, const char* key, bool fallback) noexcept
{
    yyjson_val* value = object != nullptr ? yyjson_obj_get(object, key) : nullptr;
    return value != nullptr && yyjson_is_bool(value) ? yyjson_get_bool(value) : fallback;
}

float read_float(yyjson_val* object, const char* key, float fallback) noexcept
{
    yyjson_val* value = object != nullptr ? yyjson_obj_get(object, key) : nullptr;
    return value != nullptr && yyjson_is_num(value) ? static_cast<float>(yyjson_get_num(value)) : fallback;
}

int read_int(yyjson_val* object, const char* key, int fallback) noexcept
{
    yyjson_val* value = object != nullptr ? yyjson_obj_get(object, key) : nullptr;
    if(value == nullptr || !yyjson_is_num(value)) {
        return fallback;
    }

    return yyjson_is_int(value) ? static_cast<int>(yyjson_get_sint(value)) : static_cast<int>(yyjson_get_num(value));
}

yyjson_val* read_object(yyjson_val* object, const char* key) noexcept
{
    yyjson_val* value = yyjson_obj_get(object, key);
    return value != nullptr && yyjson_is_obj(value) ? value : nullptr;
}

// The selection, or the default one when the file names something this build cannot mean - an unknown
// kind, or no id. A selection that merely points at a sky which is not on disk is kept as it is: the sky
// may come back, and skybox.cxx reports and falls back on its own.
void read_selection(yyjson_val* root, tw::skybox::config::selection& out)
{
    yyjson_val* sky = read_object(root, "sky");
    if(sky == nullptr) {
        return;
    }

    yyjson_val* kind = yyjson_obj_get(sky, "kind");
    yyjson_val* id = yyjson_obj_get(sky, "id");

    if(kind == nullptr || !yyjson_is_str(kind) || id == nullptr || !yyjson_is_str(id) || yyjson_get_len(id) == 0) {
        TW_LOG_WARNING("skybox_config: \"sky\" needs a string \"kind\" and a non-empty string \"id\" - using the default sky");
        return;
    }

    const std::string_view kind_text { yyjson_get_str(kind), yyjson_get_len(kind) };
    const std::optional<tw::skybox::entry_kind> parsed = tw::skybox::config::parse_kind(kind_text);
    if(!parsed.has_value()) {
        TW_LOG_WARNING("skybox_config: unknown sky kind '{}' - using the default sky", kind_text);
        return;
    }

    out.kind = *parsed;
    out.id.assign(yyjson_get_str(id), yyjson_get_len(id));
}

// JSON string body for `text`, which is UTF-8 already.
std::string escaped(std::string_view text)
{
    std::string out;
    out.reserve(text.size());

    for(const char c : text) {
        switch(c) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if(static_cast<unsigned char>(c) < 0x20) {
                    out += std::format("\\u{:04x}", static_cast<unsigned int>(static_cast<unsigned char>(c)));
                }
                else {
                    out += c;
                }
                break;
        }
    }

    return out;
}

void write()
{
    if(g_path.empty()) {
        return;
    }

    if(g_hold_writes) {
        TW_LOG_INFO("skybox_config: '{}' is not readable - left untouched", tw::plugin::paths::to_utf8(g_path));
        return;
    }

    std::error_code ec;
    std::filesystem::create_directories(g_path.parent_path(), ec);

    std::ofstream file { g_path, std::ios::trunc };
    if(!file.is_open()) {
        TW_LOG_ERROR("skybox_config: cannot write '{}'", tw::plugin::paths::to_utf8(g_path));
        return;
    }

    const values& v = g_values;

    file << "{\n";
    file << "  \"enabled\": " << (v.enabled ? "true" : "false") << ",\n";
    file << "  \"sky\": { \"kind\": \"" << tw::skybox::config::kind_key(v.sky.kind) << "\", \"id\": \"" << escaped(v.sky.id) << "\" },\n";
    file << "  \"cubemap\": { \"hdr_exposure\": " << std::format("{}", v.hdr_exposure) << ", \"min_face_size\": " << v.min_face_size
         << " },\n";
    file << "  \"orientation\": { \"yaw_degrees\": " << std::format("{}", v.yaw_degrees) << ", \"pitch_degrees\": "
         << std::format("{}", v.pitch_degrees) << ", \"z_up\": " << (v.z_up ? "true" : "false") << " },\n";
    file << "  \"shader\": { \"quality\": " << v.shader_quality << " }\n";
    file << "}\n";
}
} // namespace

namespace tw::skybox::config
{
void load(const std::filesystem::path& path)
{
    g_path = path;
    g_values = values {};
    g_hold_writes = false;

    std::error_code ec;
    if(!std::filesystem::exists(path, ec)) {
        // First run: write the defaults out immediately. The plugin has no unload path, so a save on
        // shutdown never comes - and a settings file nobody can find is a settings file nobody can edit.
        TW_LOG_INFO("skybox_config: '{}' not present, writing defaults", tw::plugin::paths::to_utf8(path));
        write();
        return;
    }

    std::string text;
    if(!read_file(path, text)) {
        TW_LOG_WARNING("skybox_config: cannot read '{}' - using defaults", tw::plugin::paths::to_utf8(path));
        g_hold_writes = true;
        return;
    }

    document parsed;
    yyjson_read_err error {};
    parsed.doc = yyjson_read_opts(text.data(), text.size(), k_read_flags, nullptr, &error);

    yyjson_val* root = parsed.doc != nullptr ? yyjson_doc_get_root(parsed.doc) : nullptr;
    if(root == nullptr || !yyjson_is_obj(root)) {
        TW_LOG_WARNING("skybox_config: '{}': {} at byte {} - using defaults, the file is left as it is",
            tw::plugin::paths::to_utf8(path),
            error.msg != nullptr ? error.msg : "not a JSON object",
            error.pos);
        g_hold_writes = true;
        return;
    }

    values loaded;

    loaded.enabled = read_bool(root, "enabled", loaded.enabled);
    read_selection(root, loaded.sky);

    yyjson_val* cubemap = read_object(root, "cubemap");
    loaded.hdr_exposure = read_float(cubemap, "hdr_exposure", loaded.hdr_exposure);
    loaded.min_face_size = std::max(0, read_int(cubemap, "min_face_size", loaded.min_face_size));

    yyjson_val* orientation = read_object(root, "orientation");
    loaded.yaw_degrees = read_float(orientation, "yaw_degrees", loaded.yaw_degrees);
    loaded.pitch_degrees = read_float(orientation, "pitch_degrees", loaded.pitch_degrees);
    loaded.z_up = read_bool(orientation, "z_up", loaded.z_up);

    yyjson_val* shader = read_object(root, "shader");
    loaded.shader_quality = std::clamp(read_int(shader, "quality", loaded.shader_quality), 1, 100);

    g_values = std::move(loaded);

    TW_LOG_INFO("skybox_config: loaded '{}' (enabled={} sky={}:'{}' hdr_exposure={} min_face_size={} yaw={} pitch={} z_up={} quality={})",
        tw::plugin::paths::to_utf8(path),
        g_values.enabled,
        kind_key(g_values.sky.kind),
        g_values.sky.id,
        g_values.hdr_exposure,
        g_values.min_face_size,
        g_values.yaw_degrees,
        g_values.pitch_degrees,
        g_values.z_up,
        g_values.shader_quality);
}

bool enabled() noexcept
{
    return g_values.enabled;
}

void set_enabled(bool value)
{
    g_values.enabled = value;
    g_hold_writes = false;
    write();
}

const selection& sky() noexcept
{
    return g_values.sky;
}

void select(entry_kind kind, std::string_view id)
{
    g_values.sky.kind = kind;
    g_values.sky.id.assign(id);
    g_hold_writes = false;
    write();
}

float hdr_exposure() noexcept
{
    return g_values.hdr_exposure;
}

int min_face_size() noexcept
{
    return g_values.min_face_size;
}

float yaw_degrees() noexcept
{
    return g_values.yaw_degrees;
}

float pitch_degrees() noexcept
{
    return g_values.pitch_degrees;
}

bool z_up() noexcept
{
    return g_values.z_up;
}

int shader_quality() noexcept
{
    return g_values.shader_quality;
}

void set_shader_quality(int percent)
{
    g_values.shader_quality = std::clamp(percent, 1, 100);
    g_hold_writes = false;
    write();
}

std::string_view kind_key(entry_kind kind) noexcept
{
    switch(kind) {
        case entry_kind::program:
            return "program";
        case entry_kind::shader_file:
            return "shader_file";
        case entry_kind::packed:
            return "packed";
        case entry_kind::file:
            return "file";
        case entry_kind::face_dir:
            return "face_dir";
        case entry_kind::package:
            return "package";
    }

    return "packed";
}

std::optional<entry_kind> parse_kind(std::string_view key) noexcept
{
    constexpr std::array<entry_kind, 6> k_kinds {
        entry_kind::program, entry_kind::shader_file, entry_kind::packed, entry_kind::file, entry_kind::face_dir, entry_kind::package,
    };

    for(const entry_kind kind : k_kinds) {
        if(kind_key(kind) == key) {
            return kind;
        }
    }

    return std::nullopt;
}
} // namespace tw::skybox::config
