#include "pch.hxx"

#include "skybox/skybox_config.hxx"

#include "plugin/diagnostics.hxx"

namespace
{
bool g_enabled = true;
std::string g_skybox_key = "skyboxes/cloudy_01.png";
std::string g_skybox_file;
std::string g_skybox_dir;
int g_min_face_size = 0;
float g_hdr_exposure = 1.f;
float g_yaw_degrees = 0.f;
float g_pitch_degrees = 0.f;
bool g_z_up = false;
std::string g_sky_program;
bool g_probe_markers = true;
int g_shader_quality = 100;
std::string g_loaded_path;

// Sky program parameters, in the order they were read or first set. A vector rather than a map:
// there are a handful of these, lookup happens when a shader loads rather than per frame, and the
// file keeps a stable order instead of reshuffling on every save.
std::vector<std::pair<std::string, std::string>> g_params;

// Phase 0 wrote shader_test=true to mean "run the probe". Programs subsumed it, and this is the
// one-line migration that keeps an existing .cfg meaning what it meant. Harmless to leave in - the
// key stops being written the first time the file is saved.
bool g_legacy_shader_test = false;

std::string_view trim(std::string_view s) noexcept
{
    while(!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) {
        s.remove_prefix(1);
    }
    while(!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) {
        s.remove_suffix(1);
    }
    return s;
}

bool parse_bool(std::string_view s, bool fallback) noexcept
{
    if(s == "true" || s == "1" || s == "yes") {
        return true;
    }
    if(s == "false" || s == "0" || s == "no") {
        return false;
    }
    return fallback;
}

float parse_float(std::string_view s, float fallback) noexcept
{
    float value = fallback;
    std::from_chars(s.data(), s.data() + s.size(), value);
    return value;
}

int parse_int(std::string_view s, int fallback) noexcept
{
    int value = fallback;
    std::from_chars(s.data(), s.data() + s.size(), value);
    return value;
}
} // namespace

namespace tw::skybox::config
{
void load(std::string_view path)
{
    g_loaded_path.assign(path);

    std::ifstream file { g_loaded_path };
    if(!file.is_open()) {
        // First run: write the defaults out immediately rather than waiting for a save() that will
        // never come. The plugin has no unload path (see CLAUDE.md), so shutdown() does not run in
        // practice - and a settings file nobody can find is a settings file nobody can edit.
        TW_LOG_INFO("skybox_config: '{}' not present, writing defaults", g_loaded_path);
        save();
        return;
    }

    std::string line;
    while(std::getline(file, line)) {
        // The file save() writes carries explanatory comments, and some of them contain an '='.
        if(const std::string_view trimmed = trim(line); trimmed.empty() || trimmed.front() == '#') {
            continue;
        }

        const auto eq = line.find('=');
        if(eq == std::string::npos) {
            continue;
        }

        const std::string_view key = trim(std::string_view { line }.substr(0, eq));
        const std::string_view value = trim(std::string_view { line }.substr(eq + 1));

        if(key == "enabled") {
            g_enabled = parse_bool(value, g_enabled);
        }
        else if(key == "skybox") {
            g_skybox_key.assign(value);
        }
        else if(key == "skybox_file") {
            g_skybox_file.assign(value);
        }
        else if(key == "skybox_dir") {
            g_skybox_dir.assign(value);
        }
        else if(key == "hdr_exposure") {
            g_hdr_exposure = parse_float(value, g_hdr_exposure);
        }
        else if(key == "min_face_size") {
            g_min_face_size = parse_int(value, g_min_face_size);
        }
        else if(key == "yaw_degrees") {
            g_yaw_degrees = parse_float(value, g_yaw_degrees);
        }
        else if(key == "pitch_degrees") {
            g_pitch_degrees = parse_float(value, g_pitch_degrees);
        }
        else if(key == "z_up") {
            g_z_up = parse_bool(value, g_z_up);
        }
        else if(key == "sky_program") {
            g_sky_program.assign(value);
        }
        else if(key == "shader_quality") {
            g_shader_quality = parse_int(value, g_shader_quality);
        }
        else if(key == "probe_markers" || key == "shader_test_markers") {
            g_probe_markers = parse_bool(value, g_probe_markers);
        }
        else if(key.starts_with("param.")) {
            set_param_value(key, value);
        }
        else if(key == "shader_test") {
            g_legacy_shader_test = parse_bool(value, false);
        }
    }

    if(g_legacy_shader_test && g_sky_program.empty()) {
        g_sky_program = "probe";
        TW_LOG_INFO("skybox_config: migrated shader_test=true to sky_program=probe");
    }

    // skybox_file is printed on its own line, and printed even when empty, because "is my file even
    // being read" is the first question anyone has when a skybox does not appear - and an empty
    // value here means the packed resource above is what will load, which looks identical to
    // "skybox_file was ignored".
    TW_LOG_INFO("skybox_config: loaded '{}' (enabled={} skybox='{}' min_face_size={} hdr_exposure={} yaw={} pitch={} z_up={})",
        g_loaded_path,
        g_enabled,
        g_skybox_key,
        g_min_face_size,
        g_hdr_exposure,
        g_yaw_degrees,
        g_pitch_degrees,
        g_z_up);

    if(!g_sky_program.empty()) {
        TW_LOG_INFO("skybox_config: sky_program='{}' - a shader draws the sky, both image keys are ignored", g_sky_program);
    }
    else if(g_skybox_file.empty()) {
        TW_LOG_INFO("skybox_config: skybox_file is empty - using the packed resource '{}'", g_skybox_key);
    }
    else {
        TW_LOG_INFO("skybox_config: skybox_file='{}' - this overrides the packed resource", g_skybox_file);
    }
}

void save()
{
    if(g_loaded_path.empty()) {
        return;
    }

    std::ofstream file { g_loaded_path, std::ios::trunc };
    if(!file.is_open()) {
        return;
    }

    file << "enabled=" << (g_enabled ? "true" : "false") << '\n';
    file << "skybox=" << g_skybox_key << '\n';
    file << "# skybox_file overrides `skybox`: a cross image, or a folder holding six square faces\n";
    file << "# (posx/negx/posy/negy/posz/negz, or px/nx/..., or right/left/top/bottom/front/back).\n";
    file << "# Relative to this file's folder. This is how to use art sharper than the bundled 512px.\n";
    file << "skybox_file=" << g_skybox_file << '\n';
    file << "# skybox_dir is the browse root: the overlay's Skybox tab lists everything in it next to\n";
    file << "# the packed ones. One entry per direct child - an image file, or a subfolder of six faces.\n";
    file << "skybox_dir=" << g_skybox_dir << '\n';
    file << "# Upscale faces smaller than this with Catmull-Rom. 0 = off. Improves the reconstruction\n";
    file << "# filter, invents no detail, and costs (target/source)^2 memory.\n";
    file << "min_face_size=" << g_min_face_size << '\n';
    file << "# Exposure for Radiance .hdr sources, ignored for ordinary images. Raise to brighten.\n";
    file << "hdr_exposure=" << g_hdr_exposure << '\n';
    file << "yaw_degrees=" << g_yaw_degrees << '\n';
    file << "pitch_degrees=" << g_pitch_degrees << '\n';
    file << "z_up=" << (g_z_up ? "true" : "false") << '\n';
    file << "# sky_program runs a shader instead of a cube map, and overrides both keys above. It costs no\n";
    file << "# memory and is sharp at any resolution, because nothing is sampled - the sky is computed per\n";
    file << "# pixel. Built in: gradient, night, probe. Empty means the cube map path.\n";
    file << "sky_program=" << g_sky_program << '\n';
    file << "# shader_quality renders the shader sky at this percentage of the viewport and stretches it\n";
    file << "# back. 100 = native; 67, 50 and 33 trade sharpness for the cost of a heavy program.\n";
    file << "shader_quality=" << g_shader_quality << '\n';
    file << "# Axis markers for the probe program: +X red, +Y green, +Z blue, and a ring on the horizon.\n";
    file << "probe_markers=" << (g_probe_markers ? "true" : "false") << '\n';

    if(!g_params.empty()) {
        file << "# Sky program parameters, written by the overlay's Parameters popup. Keyed by program\n";
        file << "# and by register, so renaming a knob's label does not lose its value.\n";
        for(const auto& [key, value] : g_params) {
            file << key << '=' << value << '\n';
        }
    }
}

bool enabled() noexcept
{
    return g_enabled;
}

void set_enabled(bool value) noexcept
{
    g_enabled = value;
}

const std::string& skybox_key() noexcept
{
    return g_skybox_key;
}

const std::string& skybox_file() noexcept
{
    return g_skybox_file;
}

float hdr_exposure() noexcept
{
    return g_hdr_exposure;
}

const std::string& skybox_dir() noexcept
{
    return g_skybox_dir;
}

void select_packed(std::string_view resource_key)
{
    g_skybox_key.assign(resource_key);
    g_skybox_file.clear();
    g_sky_program.clear();
    save();
}

void select_file(std::string_view path)
{
    g_skybox_file.assign(path);
    g_sky_program.clear();
    save();
}

int min_face_size() noexcept
{
    return g_min_face_size;
}

float yaw_degrees() noexcept
{
    return g_yaw_degrees;
}

float pitch_degrees() noexcept
{
    return g_pitch_degrees;
}

bool z_up() noexcept
{
    return g_z_up;
}

const std::string& sky_program() noexcept
{
    return g_sky_program;
}

void select_program(std::string_view id)
{
    g_sky_program.assign(id);
    save();
}

std::string_view param_value(std::string_view key) noexcept
{
    for(const auto& entry : g_params) {
        if(entry.first == key) {
            return entry.second;
        }
    }

    return {};
}

void set_param_value(std::string_view key, std::string_view value)
{
    for(auto& entry : g_params) {
        if(entry.first == key) {
            entry.second.assign(value);
            return;
        }
    }

    g_params.emplace_back(std::string { key }, std::string { value });
}

int shader_quality() noexcept
{
    return g_shader_quality;
}

void set_shader_quality(int percent) noexcept
{
    g_shader_quality = percent;
    save();
}

bool probe_markers() noexcept
{
    return g_probe_markers;
}

void set_probe_markers(bool value) noexcept
{
    g_probe_markers = value;
    save();
}
} // namespace tw::skybox::config
