#include "pch.hxx"

#include "skybox/sky_catalog.hxx"

#include "plugin/diagnostics.hxx"

#include "resource/resource.hxx"

#include "skybox/sky_cubemap.hxx"
#include "skybox/sky_paths.hxx"
#include "skybox/sky_program.hxx"
#include "skybox/skybox_config.hxx"

#include "libstb/stb_image.h"

namespace
{
// Packed skybox resources live under this prefix; everything else packed as a texture is a UI icon
// and has no business in this list.
constexpr std::string_view k_packed_prefix = "skyboxes/";

std::vector<tw::skybox::catalog_entry> g_entries;
unsigned int g_generation = 0;

bool equals_ignore_case(std::string_view a, std::string_view b) noexcept
{
    if(a.size() != b.size()) {
        return false;
    }

    for(std::size_t i = 0; i < a.size(); ++i) {
        if(std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }

    return true;
}

// Reads only the header, so listing a folder of 2048px faces costs kilobytes rather than decoding
// a hundred megabytes of pixels nobody has asked to see yet.
bool read_header(const std::filesystem::path& path, int& out_width, int& out_height)
{
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if(ec || size == 0) {
        return false;
    }

    std::ifstream file { path, std::ios::binary };
    if(!file.is_open()) {
        return false;
    }

    // The largest header any of stb's formats needs is far inside this; reading the whole file to
    // learn its dimensions would defeat the point.
    std::array<char, 1024> head {};
    file.read(head.data(), static_cast<std::streamsize>(head.size()));
    const auto read = static_cast<int>(file.gcount());
    if(read <= 0) {
        return false;
    }

    int components = 0;
    return stbi_info_from_memory(reinterpret_cast<const stbi_uc*>(head.data()), read, &out_width, &out_height, &components) != 0;
}

// What one face would be, from an image's proportions alone: both accepted single-image layouts
// spend a quarter of their width on a face (a 4x3 cross by construction, a 2:1 panorama because a
// face covers 90 of its 360 degrees).
int face_size_from_single_image(int width, int height) noexcept
{
    if(width <= 0 || height <= 0) {
        return 0;
    }

    const bool panorama = width == height * 2;
    const bool cross = width * 3 == height * 4;

    return panorama || cross ? width / 4 : 0;
}

bool is_face_stem(std::string_view stem) noexcept
{
    for(const auto& spellings : tw::skybox::face_stems()) {
        for(const std::string_view accepted : spellings) {
            if(equals_ignore_case(stem, accepted)) {
                return true;
            }
        }
    }

    return false;
}

// A folder counts as a skybox when at least one file in it is named like a cube face. Requiring all
// six here would hide a half-finished set instead of listing it and letting the loader explain
// which face is missing.
bool scan_face_directory(const std::filesystem::path& directory, int& out_face_size)
{
    std::error_code ec;
    bool found = false;

    for(const auto& entry : std::filesystem::directory_iterator { directory, ec }) {
        if(!entry.is_regular_file() || !is_face_stem(entry.path().stem().string())) {
            continue;
        }

        found = true;

        int width = 0;
        int height = 0;
        if(read_header(entry.path(), width, height) && width == height) {
            out_face_size = width;
        }
        break;
    }

    return found && !ec;
}

void add_program_entries()
{
    for(const tw::skybox::sky_program* program : tw::skybox::programs()) {
        // Only the built-ins. A program compiled from a file is listed by the directory scan below,
        // from the file itself - otherwise a shader that has been picked once would appear twice,
        // and one that has never been picked would not appear at all.
        if(program->from_file()) {
            continue;
        }

        g_entries.push_back(tw::skybox::catalog_entry { program->display_name, program->id, tw::skybox::entry_kind::program, 0 });
    }
}

void add_packed_entries()
{
    for(const std::string_view key : tw::resource::list_keys(tw::resource::type::texture)) {
        if(!key.starts_with(k_packed_prefix)) {
            continue;
        }

        std::string name { key.substr(k_packed_prefix.size()) };
        if(const auto dot = name.find_last_of('.'); dot != std::string::npos) {
            name.resize(dot);
        }

        g_entries.push_back(tw::skybox::catalog_entry { std::move(name), std::string { key }, tw::skybox::entry_kind::packed, 0 });
    }
}

void add_directory_entries()
{
    const std::string& configured = tw::skybox::config::skybox_dir();
    if(configured.empty()) {
        return;
    }

    const std::filesystem::path root = tw::skybox::resolve_source_path(configured);
    if(root.empty()) {
        return;
    }

    std::error_code ec;
    if(!std::filesystem::is_directory(root, ec)) {
        TW_LOG_WARNING("sky_catalog: skybox_dir '{}' is not a directory", root.string());
        return;
    }

    int listed = 0;

    for(const auto& entry : std::filesystem::directory_iterator { root, ec }) {
        // Stored as the absolute path rather than a name relative to skybox_dir: the entry has to
        // survive skybox_dir being edited afterwards, and resolve_source_path returns an absolute
        // path unchanged.
        const std::string id = entry.path().string();

        if(entry.is_directory()) {
            int face_size = 0;
            if(scan_face_directory(entry.path(), face_size)) {
                g_entries.push_back(
                    tw::skybox::catalog_entry { entry.path().filename().string(), id, tw::skybox::entry_kind::face_dir, face_size });
                ++listed;
            }
            continue;
        }

        if(!entry.is_regular_file()) {
            continue;
        }

        // Listed without compiling: a scan that compiled every .hlsl it found would turn opening the
        // tab into a stall proportional to how many shaders somebody keeps in the folder. The
        // compile happens when one is picked, and its errors show up there.
        if(equals_ignore_case(entry.path().extension().string(), ".hlsl")) {
            g_entries.push_back(tw::skybox::catalog_entry { entry.path().stem().string(), id, tw::skybox::entry_kind::shader_file, 0 });
            ++listed;
            continue;
        }

        int width = 0;
        int height = 0;
        if(!read_header(entry.path(), width, height)) {
            continue;
        }

        const int face_size = face_size_from_single_image(width, height);
        if(face_size == 0) {
            TW_LOG_INFO("sky_catalog: skipping '{}' - {}x{} is neither a 4x3 cross nor a 2:1 panorama",
                entry.path().filename().string(),
                width,
                height);
            continue;
        }

        g_entries.push_back(tw::skybox::catalog_entry { entry.path().stem().string(), id, tw::skybox::entry_kind::file, face_size });
        ++listed;
    }

    if(ec) {
        TW_LOG_WARNING("sky_catalog: could not finish enumerating '{}'", root.string());
    }

    TW_LOG_INFO("sky_catalog: skybox_dir '{}' contributed {} entries", root.string(), listed);
}
} // namespace

namespace tw::skybox
{
void refresh_catalog()
{
    std::vector<catalog_entry> previous;
    previous.swap(g_entries);

    add_program_entries();
    add_packed_entries();
    add_directory_entries();

    // Packed entries come out of the resource index in unspecified order, and a list that reshuffles
    // between refreshes is worse than useless to click on. Sorted by kind first so each group stays
    // together, then by name - except the programs, which keep the order sky_program declares them
    // in: that table is short and deliberately ordered, with the diagnostic last.
    std::stable_sort(g_entries.begin(), g_entries.end(), [](const catalog_entry& a, const catalog_entry& b) {
        if(a.kind != b.kind) {
            return a.kind < b.kind;
        }
        if(a.kind == entry_kind::program) {
            return false;
        }
        return a.display_name < b.display_name;
    });

    const bool changed =
        previous.size() != g_entries.size()
        || !std::equal(previous.begin(), previous.end(), g_entries.begin(), [](const catalog_entry& a, const catalog_entry& b) {
               return a.kind == b.kind && a.id == b.id && a.face_size == b.face_size;
           });

    if(changed) {
        ++g_generation;
        TW_LOG_INFO("sky_catalog: {} skyboxes available", g_entries.size());
    }
}

std::span<const catalog_entry> catalog() noexcept
{
    return g_entries;
}

unsigned int catalog_generation() noexcept
{
    return g_generation;
}

int selected_catalog_index() noexcept
{
    const std::string& program = config::sky_program();
    const std::string& file = config::skybox_file();
    const std::string& packed = config::skybox_key();

    for(std::size_t i = 0; i < g_entries.size(); ++i) {
        const catalog_entry& entry = g_entries[i];

        // A program wins over both image keys, exactly as it does in the draw path - config clears
        // the others when one is picked, so this only has to agree about which key is authoritative.
        if(!program.empty()) {
            if(entry.kind == entry_kind::program && entry.id == program) {
                return static_cast<int>(i);
            }

            // A file shader's id is an absolute path while the config may hold a relative one, so
            // the two are compared through the same resolver, as the image paths below are.
            if(entry.kind == entry_kind::shader_file) {
                std::error_code ec;
                if(std::filesystem::equivalent(entry.id, resolve_source_path(program), ec)) {
                    return static_cast<int>(i);
                }
            }
            continue;
        }

        if(entry.kind == entry_kind::program || entry.kind == entry_kind::shader_file) {
            continue;
        }

        if(!file.empty()) {
            // The config may hold a relative path while the catalog holds absolute ones, so compare
            // through the same resolver both sides went through.
            if(entry.kind != entry_kind::packed) {
                std::error_code ec;
                if(std::filesystem::equivalent(entry.id, resolve_source_path(file), ec)) {
                    return static_cast<int>(i);
                }
            }
            continue;
        }

        if(entry.kind == entry_kind::packed && entry.id == packed) {
            return static_cast<int>(i);
        }
    }

    return -1;
}
} // namespace tw::skybox
