#include "pch.hxx"

#include "skybox/sky_vfs.hxx"

#include "plugin/diagnostics.hxx"

#include "libminiz/miniz.h"

namespace
{
// The largest single file this will pull out of an archive.
//
// A zip states its own uncompressed sizes, and a hostile one may state anything: the classic bomb is
// a few kilobytes that expand to gigabytes. Nothing a sky legitimately ships comes close - a 4096
// square texture is sixty-four megabytes decoded and far less on disk - so a cap here costs nothing
// real and turns "the game stopped responding" into a line in the log.
constexpr std::uint64_t k_max_entry_bytes = 128ull * 1024ull * 1024ull;

// The DOS device names, which Win32 still resolves at any depth in a path and which open
// successfully - `NUL` most usefully of all. A component matches when its stem, before the first
// dot, is one of these: `CON.txt` is still the console.
constexpr std::array<std::string_view, 22> k_device_names { {
    "CON",
    "PRN",
    "AUX",
    "NUL",
    "COM1",
    "COM2",
    "COM3",
    "COM4",
    "COM5",
    "COM6",
    "COM7",
    "COM8",
    "COM9",
    "LPT1",
    "LPT2",
    "LPT3",
    "LPT4",
    "LPT5",
    "LPT6",
    "LPT7",
    "LPT8",
    "LPT9",
} };

bool equals_ignore_case(std::string_view a, std::string_view b) noexcept
{
    if(a.size() != b.size()) {
        return false;
    }

    for(std::size_t i = 0; i < a.size(); ++i) {
        if(std::toupper(static_cast<unsigned char>(a[i])) != std::toupper(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }

    return true;
}

bool is_device_component(std::string_view component) noexcept
{
    const std::size_t dot = component.find('.');
    const std::string_view stem = dot == std::string_view::npos ? component : component.substr(0, dot);

    for(const std::string_view device : k_device_names) {
        if(equals_ignore_case(stem, device)) {
            return true;
        }
    }

    return false;
}

// Splits on both separators, because a manifest writes `/` and a hand-edited one may well contain
// `\`. Empty components (a doubled separator) are dropped rather than rejected - they are a typo,
// not an attack, and `shaders//clouds` means what it looks like.
bool for_each_component(std::string_view name, const std::function<bool(std::string_view)>& visit)
{
    std::size_t start = 0;

    while(start <= name.size()) {
        std::size_t at = name.find_first_of("/\\", start);
        if(at == std::string_view::npos) {
            at = name.size();
        }

        const std::string_view component = name.substr(start, at - start);
        if(!component.empty() && !visit(component)) {
            return false;
        }

        if(at == name.size()) {
            break;
        }
        start = at + 1;
    }

    return true;
}

// Whether `candidate` lies inside `root`, comparing component by component.
//
// Not a string prefix test: "/skies/foo" is a prefix of "/skies/foo-evil" as text, and of nothing at
// all as a path. Both sides must already be canonical.
bool is_inside(const std::filesystem::path& root, const std::filesystem::path& candidate) noexcept
{
    auto root_it = root.begin();
    auto cand_it = candidate.begin();

    for(; root_it != root.end(); ++root_it, ++cand_it) {
        if(cand_it == candidate.end()) {
            return false;
        }

        // Case-insensitive because the filesystem is: a package reached as C:\Skies and a file
        // resolved to c:\skies are the same place, and treating them as different would reject a
        // legitimate read rather than admit an illegitimate one.
        if(!equals_ignore_case(root_it->string(), cand_it->string())) {
            return false;
        }
    }

    return true;
}

// Splits a name into lowercased components, dropping `.` and empty ones. False when any component is
// `..`, which inside an archive is not an escape - nothing is opened by path - but is still a name
// with two spellings, and a lookup table that answers to both is a lookup table that can be confused.
bool split_components(std::string_view name, std::vector<std::string>& out)
{
    out.clear();

    bool ok = true;

    for_each_component(name, [&out, &ok](std::string_view component) {
        if(component == ".") {
            return true;
        }

        if(component == "..") {
            ok = false;
            return false;
        }

        std::string lowered;
        lowered.reserve(component.size());

        for(const char c : component) {
            lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }

        out.push_back(std::move(lowered));

        return true;
    });

    return ok && !out.empty();
}

std::string join_components(std::span<const std::string> components)
{
    std::string out;

    for(const std::string& component : components) {
        if(!out.empty()) {
            out.push_back('/');
        }
        out.append(component);
    }

    return out;
}

// The key an archive's index is looked up by: lowercased, `/`-separated, no `.` and no `..`.
//
// Lowercased because a zip stores whatever case the author's filesystem had, while a manifest is
// written by hand - the two agree about "shaders/Clouds.ps.hlsl" only if neither has to.
std::string archive_key(std::string_view name)
{
    std::vector<std::string> components;
    if(!split_components(name, components)) {
        return {};
    }

    return join_components(components);
}

bool read_whole_file(const std::filesystem::path& path, std::vector<std::byte>& out)
{
    out.clear();

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
        file.read(reinterpret_cast<char*>(out.data()), size);
    }

    return true;
}
} // namespace

namespace tw::skybox::package
{
// The zip behind an archive package. One per opened package, shared by every copy of the
// file_system that names it.
struct archive {
    std::filesystem::path path;
    std::filesystem::file_time_type write_time {};

    // The whole archive, held for as long as the reader is. miniz was initialised over this buffer
    // and reads from it on every extraction, so it must not move - which is why this lives in a
    // struct nothing copies rather than being handed around.
    std::vector<std::byte> bytes;

    mz_zip_archive zip {};
    bool opened {};

    // Normalised entry name to its index in the archive. Built once, because miniz's own lookup is a
    // linear scan of the central directory and a package asks for a dozen files by name.
    std::unordered_map<std::string, mz_uint> index;

    // miniz's reader keeps state in the mz_zip_archive across a call, so two threads extracting at
    // once would corrupt it. Reads here are cold - loading a sky, compiling its shaders - so a plain
    // lock is the right shape and the contention is theoretical.
    std::mutex mutex;

    archive() = default;

    ~archive()
    {
        if(opened) {
            mz_zip_reader_end(&zip);
        }
    }

    archive(const archive&) = delete;
    archive& operator=(const archive&) = delete;
};

bool is_safe_relative_name(std::string_view name) noexcept
{
    if(name.empty()) {
        return false;
    }

    // Absolute in every spelling Win32 accepts: a leading separator, a UNC path, and the
    // drive-relative `C:foo` - which is neither absolute nor relative to anything we chose.
    if(name.front() == '/' || name.front() == '\\') {
        return false;
    }

    if(name.size() >= 2 && name[1] == ':') {
        return false;
    }

    bool ok = true;

    for_each_component(name, [&ok](std::string_view component) {
        // `..` is the obvious one, and `.` is dropped for tidiness rather than danger.
        if(component == ".." || component == ".") {
            ok = component == ".";
            return ok;
        }

        // An alternate data stream rides on a colon: `clouds.png:payload` reads a hidden stream of a
        // file whose visible name looks perfectly ordinary.
        if(component.find(':') != std::string_view::npos) {
            ok = false;
            return false;
        }

        if(is_device_component(component)) {
            ok = false;
            return false;
        }

        // A trailing space or dot is stripped by Win32 path normalisation, so "clouds.png " and
        // "clouds.png" name the same file while comparing as different strings. Refused rather than
        // trimmed: nothing legitimate is named that way.
        if(component.back() == ' ' || component.back() == '.') {
            ok = false;
            return false;
        }

        return true;
    });

    return ok;
}

file_system file_system::open_directory(const std::filesystem::path& root)
{
    file_system out;

    std::error_code ec;
    std::filesystem::path canonical = std::filesystem::weakly_canonical(root, ec);

    if(ec || canonical.empty()) {
        TW_LOG_WARNING("sky_vfs: cannot canonicalise package root '{}'", root.string());
        return out;
    }

    if(!std::filesystem::is_directory(canonical, ec)) {
        TW_LOG_WARNING("sky_vfs: package root '{}' is not a directory", canonical.string());
        return out;
    }

    out.m_root = std::move(canonical);

    return out;
}

file_system file_system::open_archive(const std::filesystem::path& file)
{
    file_system out;

    std::error_code ec;
    std::filesystem::path canonical = std::filesystem::weakly_canonical(file, ec);

    if(ec || canonical.empty() || !std::filesystem::is_regular_file(canonical, ec)) {
        TW_LOG_WARNING("sky_vfs: '{}' is not a readable file", file.string());
        return out;
    }

    auto opened = std::make_shared<archive>();
    opened->path = canonical;
    opened->write_time = std::filesystem::last_write_time(canonical, ec);

    if(ec) {
        opened->write_time = {};
    }

    if(!read_whole_file(canonical, opened->bytes) || opened->bytes.empty()) {
        TW_LOG_WARNING("sky_vfs: could not read '{}'", canonical.string());
        return out;
    }

    if(mz_zip_reader_init_mem(&opened->zip, opened->bytes.data(), opened->bytes.size(), 0) == MZ_FALSE) {
        TW_LOG_WARNING("sky_vfs: '{}' is not a zip archive", canonical.string());
        return out;
    }

    opened->opened = true;

    // Every entry's components, kept while the common prefix below is decided. Two passes rather than
    // one because whether a top-level folder exists is a property of the whole archive, and the first
    // entry cannot answer it.
    const mz_uint count = mz_zip_reader_get_num_files(&opened->zip);

    std::vector<std::vector<std::string>> entries;
    std::vector<mz_uint> indices;

    entries.reserve(count);
    indices.reserve(count);

    std::string name;
    int rejected = 0;

    for(mz_uint i = 0; i < count; ++i) {
        if(mz_zip_reader_is_file_a_directory(&opened->zip, i) != MZ_FALSE) {
            continue;
        }

        const mz_uint length = mz_zip_reader_get_filename(&opened->zip, i, nullptr, 0);
        if(length == 0) {
            continue;
        }

        name.assign(length, '\0');
        mz_zip_reader_get_filename(&opened->zip, i, name.data(), length);

        // get_filename writes a NUL terminator inside the buffer it was given the length of.
        while(!name.empty() && name.back() == '\0') {
            name.pop_back();
        }

        std::vector<std::string> components;
        if(!split_components(name, components) || !is_safe_relative_name(name)) {
            ++rejected;
            continue;
        }

        entries.push_back(std::move(components));
        indices.push_back(i);
    }

    // Zipping a package folder and zipping its contents are both what people do, and an author who
    // did the first should not be told their sky has no manifest. Stripped only when *every* entry
    // agrees and none of them sits at the top level - two top-level folders, or a Config.json beside
    // one, mean the archive is already rooted where it claims.
    bool strip_prefix = !entries.empty();

    for(const std::vector<std::string>& components : entries) {
        if(components.size() < 2 || components.front() != entries.front().front()) {
            strip_prefix = false;
            break;
        }
    }

    for(std::size_t i = 0; i < entries.size(); ++i) {
        std::span<const std::string> components { entries[i] };
        if(strip_prefix) {
            components = components.subspan(1);
        }

        // First spelling wins. A zip may hold two entries whose names differ only in case, and on a
        // case-insensitive filesystem the folder form of the same package would only have one of
        // them - so taking the first keeps the two forms answering alike.
        opened->index.emplace(join_components(components), indices[i]);
    }

    if(rejected > 0) {
        TW_LOG_WARNING("sky_vfs: '{}' has {} entries with names that cannot be addressed - they were skipped",
            canonical.string(),
            rejected);
    }

    TW_LOG_INFO("sky_vfs: opened archive '{}': {} readable entries{}",
        canonical.string(),
        opened->index.size(),
        strip_prefix ? ", rooted one folder in" : "");

    out.m_root = std::move(canonical);
    out.m_archive = std::move(opened);

    return out;
}

std::filesystem::path file_system::locate(std::string_view name) const
{
    // An archive has no paths, and this is where that is decided once. Callers that need bytes call
    // read(); the ones that call this are the ones that genuinely wanted a file on disk, and for an
    // archive the honest answer is that there is not one.
    if(m_archive != nullptr) {
        return {};
    }

    if(m_root.empty() || !is_safe_relative_name(name)) {
        return {};
    }

    std::error_code ec;

    // weakly_canonical, so this resolves symlinks and junctions - the one escape the text check
    // above cannot see. A link inside the package pointing at C:\Windows canonicalises to a path
    // outside the root, and the comparison below is what refuses it.
    const std::filesystem::path resolved = std::filesystem::weakly_canonical(m_root / std::filesystem::path { name }, ec);
    if(ec || resolved.empty()) {
        return {};
    }

    if(!is_inside(m_root, resolved)) {
        TW_LOG_WARNING("sky_vfs: '{}' resolves outside its package and was refused", std::string { name });
        return {};
    }

    return resolved;
}

bool file_system::exists(std::string_view name) const
{
    if(m_archive != nullptr) {
        if(!is_safe_relative_name(name)) {
            return false;
        }

        const std::string key = archive_key(name);

        return !key.empty() && m_archive->index.contains(key);
    }

    const std::filesystem::path path = locate(name);
    if(path.empty()) {
        return false;
    }

    std::error_code ec;

    return std::filesystem::is_regular_file(path, ec);
}

std::filesystem::file_time_type file_system::write_time(std::string_view name) const
{
    if(m_archive != nullptr) {
        // The archive's own stamp, and only when the name is actually in it. Everything inside a zip
        // changes together and only when the zip is rewritten, so per-entry times would be a
        // distinction the reload watch cannot act on.
        return exists(name) ? m_archive->write_time : std::filesystem::file_time_type {};
    }

    const std::filesystem::path path = locate(name);
    if(path.empty()) {
        return {};
    }

    std::error_code ec;
    const std::filesystem::file_time_type stamp = std::filesystem::last_write_time(path, ec);

    return ec ? std::filesystem::file_time_type {} : stamp;
}

bool file_system::read(std::string_view name, std::vector<std::byte>& out) const
{
    out.clear();

    if(m_archive != nullptr) {
        if(!is_safe_relative_name(name)) {
            return false;
        }

        const std::string key = archive_key(name);
        if(key.empty()) {
            return false;
        }

        const std::lock_guard<std::mutex> guard { m_archive->mutex };

        const auto found = m_archive->index.find(key);
        if(found == m_archive->index.end()) {
            return false;
        }

        mz_zip_archive_file_stat stat {};
        if(mz_zip_reader_file_stat(&m_archive->zip, found->second, &stat) == MZ_FALSE) {
            return false;
        }

        if(stat.m_uncomp_size > k_max_entry_bytes) {
            TW_LOG_WARNING("sky_vfs: '{}' in '{}' claims to be {} bytes and was refused",
                std::string { name },
                m_archive->path.string(),
                stat.m_uncomp_size);
            return false;
        }

        out.resize(static_cast<std::size_t>(stat.m_uncomp_size));
        if(out.empty()) {
            return true;
        }

        if(mz_zip_reader_extract_to_mem(&m_archive->zip, found->second, out.data(), out.size(), 0) == MZ_FALSE) {
            out.clear();
            return false;
        }

        return true;
    }

    const std::filesystem::path path = locate(name);
    if(path.empty()) {
        return false;
    }

    std::error_code ec;

    // Checked rather than left to the open below, because opening a directory succeeds on some
    // configurations and then reads zero bytes, which would look like an empty file.
    if(!std::filesystem::is_regular_file(path, ec)) {
        return false;
    }

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
        file.read(reinterpret_cast<char*>(out.data()), size);
    }

    return true;
}

bool file_system::read_text(std::string_view name, std::string& out) const
{
    std::vector<std::byte> bytes;
    if(!read(name, bytes)) {
        out.clear();
        return false;
    }

    out.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());

    return true;
}
} // namespace tw::skybox::package
