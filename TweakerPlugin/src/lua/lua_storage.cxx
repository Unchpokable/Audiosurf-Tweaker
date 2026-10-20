#include "pch.hxx"

#include "lua/lua_storage.hxx"

#include "lua/lua_diag.hxx"
#include "lua/lua_registry.hxx"

#include <libyyjson/yyjson.h>

namespace
{
namespace storage = tw::lua::storage;

// How often the writer thread wakes - also the most a file on disk can be behind what the script wrote.
constexpr DWORD k_writer_period_ms = 100;

// The temporary a file is written through. '~' is not a name a script can use, so it never collides with one.
constexpr std::string_view k_temp_suffix = "~tmp";

struct file_entry {
    std::string name;
    std::string bytes;   // the content, once `loaded`
    std::size_t size = 0; // what it is or will be on disk - for the limits
    bool loaded = false;  // `bytes` holds the content: read from disk, or written by the script
    bool exists = false;  // on disk or written, and not removed
    bool dirty = false;   // the writer has something to do: write it (exists) or delete it (!exists)
};

struct owner_data {
    std::filesystem::path dir;
    std::vector<file_entry> files;
};

std::filesystem::path g_root;

// By script id; null for a script that has not used tw.storage since it was loaded.
std::vector<std::unique_ptr<owner_data>> g_owners;

// g_owners and everything in it, between the scripts' thread and the writer. The scripts' thread holds it for
// the length of one call; the writer for as long as it takes to copy out what is dirty.
SRWLOCK g_lock = SRWLOCK_INIT;

// One pass of writing at a time: the writer's, or a synchronous one (a script unloading, shutdown), never two at
// once on the same temporary file.
SRWLOCK g_io_lock = SRWLOCK_INIT;

// What the writer could not write, for pump() to put in front of the script - lua_diag is not thread-safe.
std::vector<std::pair<int, std::string>> g_failed;
std::atomic<bool> g_any_failed { false };

HANDLE g_stop = nullptr;
HANDLE g_writer = nullptr;

// read_json's tape and its strings, until the next call; write_json's text.
std::vector<storage::json_node> g_tape;
std::string g_pool;
std::string g_text;

file_entry* find_file(owner_data& data, std::string_view name) noexcept
{
    for(file_entry& file : data.files) {
        if(file.name == name) {
            return &file;
        }
    }
    return nullptr;
}

std::string system_message(DWORD code)
{
    return std::format("Windows error {}", code);
}

// The whole file, through the wide path. Refuses one larger than a script may have.
bool read_file(const std::filesystem::path& path, std::string& out, std::string& error)
{
    const HANDLE file = ::CreateFileW(path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if(file == INVALID_HANDLE_VALUE) {
        error = std::format("cannot open it ({})", system_message(::GetLastError()));
        return false;
    }

    LARGE_INTEGER size {};
    bool ok = ::GetFileSizeEx(file, &size) != 0;
    if(ok && static_cast<unsigned long long>(size.QuadPart) > storage::k_max_file) {
        error = std::format("it is {} bytes - files are read up to {}", size.QuadPart, storage::k_max_file);
        ok = false;
    }
    else if(!ok) {
        error = std::format("cannot read it ({})", system_message(::GetLastError()));
    }

    if(ok) {
        out.resize(static_cast<std::size_t>(size.QuadPart));
        DWORD read = 0;
        ok = out.empty() || (::ReadFile(file, out.data(), static_cast<DWORD>(out.size()), &read, nullptr) != 0 && read == out.size());
        if(!ok) {
            error = std::format("cannot read it ({})", system_message(::GetLastError()));
        }
    }

    ::CloseHandle(file);
    return ok;
}

// Through a temporary and a rename: a game that dies in the middle leaves the old file or the new one.
bool write_file(const std::filesystem::path& dir, const std::string& name, const std::string& bytes, std::string& error)
{
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);

    const std::filesystem::path target = dir / std::filesystem::path { name };
    const std::filesystem::path temp = dir / std::filesystem::path { name + std::string { k_temp_suffix } };

    const HANDLE file = ::CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if(file == INVALID_HANDLE_VALUE) {
        error = std::format("{} was not saved: cannot create it ({})", name, system_message(::GetLastError()));
        return false;
    }

    DWORD written = 0;
    const bool ok = bytes.empty() || (::WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) != 0 && written == bytes.size());
    const DWORD write_error = ::GetLastError();
    ::CloseHandle(file);

    if(!ok) {
        ::DeleteFileW(temp.c_str());
        error = std::format("{} was not saved: {}", name, system_message(write_error));
        return false;
    }

    if(::MoveFileExW(temp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING) == 0) {
        const DWORD move_error = ::GetLastError();
        ::DeleteFileW(temp.c_str());
        error = std::format("{} was not saved: cannot replace it ({})", name, system_message(move_error));
        return false;
    }

    return true;
}

bool delete_file(const std::filesystem::path& dir, const std::string& name, std::string& error)
{
    const std::filesystem::path target = dir / std::filesystem::path { name };
    if(::DeleteFileW(target.c_str()) != 0) {
        return true;
    }

    const DWORD code = ::GetLastError();
    if(code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND) {
        return true;
    }
    error = std::format("{} was not removed ({})", name, system_message(code));
    return false;
}

// The script's folder, listed on its first use of tw.storage since it was loaded - what the limits count. Names a
// script could not have written (a temporary left by a crash, a file the user dropped in under another name) are
// not its files.
owner_data* touch(int owner, std::string& error)
{
    const tw::lua::script* const script = tw::lua::registry::find(owner);
    if(script == nullptr) {
        error = "only a script has a storage folder";
        return nullptr;
    }
    if(g_root.empty()) {
        error = "there is no TweakerStuff folder";
        return nullptr;
    }

    if(owner >= static_cast<int>(g_owners.size())) {
        g_owners.resize(static_cast<std::size_t>(owner) + 1);
    }

    std::unique_ptr<owner_data>& slot = g_owners[static_cast<std::size_t>(owner)];
    if(slot != nullptr) {
        return slot.get();
    }

    slot = std::make_unique<owner_data>();
    slot->dir = g_root / script->path.stem();

    std::error_code ec;
    for(const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(slot->dir, ec)) {
        std::error_code entry_ec;
        if(!entry.is_regular_file(entry_ec)) {
            continue;
        }

        const std::wstring wide = entry.path().filename().wstring();
        std::string name;
        name.reserve(wide.size());
        bool ascii = true;
        for(const wchar_t c : wide) {
            ascii = ascii && c < 0x80;
            name.push_back(static_cast<char>(c));
        }
        if(!ascii || !storage::valid_name(name)) {
            continue;
        }

        file_entry file;
        file.name = std::move(name);
        const std::uintmax_t size = entry.file_size(entry_ec);
        file.size = entry_ec ? 0 : static_cast<std::size_t>(size);
        file.exists = true;
        slot->files.push_back(std::move(file));
    }

    return slot.get();
}

struct job {
    int owner;
    std::filesystem::path dir;
    std::string name;
    std::string bytes;
    bool remove;
};

// One pass of writing: what is dirty is copied out under the lock, then written with the lock released, so a
// script's next write never waits for the disk. `only` limits it to one script (-1: all). A failure is reported
// straight away on the scripts' thread, or left for pump() from the writer's.
void flush_pass(int only, bool on_scripts_thread)
{
    ::AcquireSRWLockExclusive(&g_io_lock);

    std::vector<job> jobs;
    ::AcquireSRWLockExclusive(&g_lock);
    for(std::size_t owner = 0; owner < g_owners.size(); ++owner) {
        owner_data* const data = g_owners[owner].get();
        if(data == nullptr || (only >= 0 && static_cast<int>(owner) != only)) {
            continue;
        }
        for(file_entry& file : data->files) {
            if(file.dirty) {
                jobs.push_back({ static_cast<int>(owner), data->dir, file.name, file.exists ? file.bytes : std::string {}, !file.exists });
                file.dirty = false;
            }
        }
    }
    ::ReleaseSRWLockExclusive(&g_lock);

    for(const job& item : jobs) {
        std::string error;
        const bool ok = item.remove ? delete_file(item.dir, item.name, error) : write_file(item.dir, item.name, item.bytes, error);
        if(ok) {
            continue;
        }

        if(on_scripts_thread) {
            tw::lua::diag::report(item.owner, tw::lua::diag::level::error, "tw.storage", error);
        }
        else {
            ::AcquireSRWLockExclusive(&g_lock);
            g_failed.emplace_back(item.owner, std::move(error));
            ::ReleaseSRWLockExclusive(&g_lock);
            g_any_failed.store(true, std::memory_order_release);
        }
    }

    ::ReleaseSRWLockExclusive(&g_io_lock);
}

DWORD WINAPI writer_main(void*)
{
    while(::WaitForSingleObject(g_stop, k_writer_period_ms) == WAIT_TIMEOUT) {
        flush_pass(-1, false);
    }
    return 0;
}

// --- JSON ------------------------------------------------------------------------------------------------------

bool push_tape(yyjson_val* value, int depth, std::vector<storage::json_node>& tape, std::string& pool, std::string& error)
{
    if(depth > storage::k_max_depth) {
        error = std::format("it nests deeper than {} levels", storage::k_max_depth);
        return false;
    }

    // Strings go into the pool by offset - kept in `count` until the tape is complete, because the pool moves
    // as it grows - and get their pointers at the end.
    const auto push_string = [&](const char* text, std::size_t length) {
        tape.push_back({ static_cast<int>(storage::json_type::string), static_cast<int>(pool.size()), 0.0, nullptr, static_cast<int>(length) });
        pool.append(text, length);
    };

    switch(yyjson_get_type(value)) {
        case YYJSON_TYPE_NULL:
            tape.push_back({ static_cast<int>(storage::json_type::null), 0, 0.0, nullptr, 0 });
            return true;
        case YYJSON_TYPE_BOOL:
            tape.push_back({ static_cast<int>(yyjson_get_bool(value) ? storage::json_type::true_value : storage::json_type::false_value), 0, 0.0, nullptr, 0 });
            return true;
        case YYJSON_TYPE_NUM:
            tape.push_back({ static_cast<int>(storage::json_type::number), 0, yyjson_get_num(value), nullptr, 0 });
            return true;
        case YYJSON_TYPE_STR:
            push_string(yyjson_get_str(value), yyjson_get_len(value));
            return true;
        case YYJSON_TYPE_ARR: {
            tape.push_back({ static_cast<int>(storage::json_type::array), static_cast<int>(yyjson_arr_size(value)), 0.0, nullptr, 0 });
            yyjson_arr_iter it = yyjson_arr_iter_with(value);
            while(yyjson_val* const child = yyjson_arr_iter_next(&it)) {
                if(!push_tape(child, depth + 1, tape, pool, error)) {
                    return false;
                }
            }
            return true;
        }
        case YYJSON_TYPE_OBJ: {
            tape.push_back({ static_cast<int>(storage::json_type::object), static_cast<int>(yyjson_obj_size(value)), 0.0, nullptr, 0 });
            yyjson_obj_iter it = yyjson_obj_iter_with(value);
            while(yyjson_val* const key = yyjson_obj_iter_next(&it)) {
                push_string(yyjson_get_str(key), yyjson_get_len(key));
                if(!push_tape(yyjson_obj_iter_get_val(key), depth + 1, tape, pool, error)) {
                    return false;
                }
            }
            return true;
        }
        default:
            error = "it holds a value JSON does not have";
            return false;
    }
}

yyjson_mut_val* build_value(yyjson_mut_doc* doc, std::span<const storage::json_node> tape, std::size_t& at, int depth, std::string& error)
{
    if(at >= tape.size()) {
        error = "the value is incomplete";
        return nullptr;
    }
    if(depth > storage::k_max_depth) {
        error = std::format("it nests deeper than {} levels", storage::k_max_depth);
        return nullptr;
    }

    const storage::json_node& node = tape[at++];
    switch(static_cast<storage::json_type>(node.type)) {
        case storage::json_type::null:
            return yyjson_mut_null(doc);
        case storage::json_type::false_value:
            return yyjson_mut_bool(doc, false);
        case storage::json_type::true_value:
            return yyjson_mut_bool(doc, true);
        case storage::json_type::number: {
            const double v = node.number;
            if(!std::isfinite(v)) {
                error = "NaN and infinity have no JSON";
                return nullptr;
            }
            // Whole numbers as integers - 3, not 3.0 - as far as a double holds them exactly.
            if(v == std::floor(v) && std::fabs(v) <= 9007199254740992.0) {
                return yyjson_mut_sint(doc, static_cast<std::int64_t>(v));
            }
            return yyjson_mut_real(doc, v);
        }
        case storage::json_type::string:
            if(node.len < 0 || (node.str == nullptr && node.len > 0)) {
                error = "a string is malformed";
                return nullptr;
            }
            return yyjson_mut_strncpy(doc, node.str != nullptr ? node.str : "", static_cast<std::size_t>(node.len));
        case storage::json_type::array: {
            yyjson_mut_val* const array = yyjson_mut_arr(doc);
            for(int i = 0; i < node.count; ++i) {
                yyjson_mut_val* const child = build_value(doc, tape, at, depth + 1, error);
                if(child == nullptr) {
                    return nullptr;
                }
                yyjson_mut_arr_append(array, child);
            }
            return array;
        }
        case storage::json_type::object: {
            yyjson_mut_val* const object = yyjson_mut_obj(doc);
            for(int i = 0; i < node.count; ++i) {
                if(at >= tape.size() || tape[at].type != static_cast<int>(storage::json_type::string)) {
                    error = "an object's key is not a string";
                    return nullptr;
                }
                yyjson_mut_val* const key = build_value(doc, tape, at, depth + 1, error);
                yyjson_mut_val* const child = key != nullptr ? build_value(doc, tape, at, depth + 1, error) : nullptr;
                if(child == nullptr) {
                    return nullptr;
                }
                yyjson_mut_obj_add(object, key, child);
            }
            return object;
        }
    }

    error = "the value is of no JSON type";
    return nullptr;
}
} // namespace

namespace tw::lua::storage
{
bool valid_name(std::string_view name) noexcept
{
    if(name.empty() || name.size() > k_max_name || name.front() == '.') {
        return false;
    }

    return std::ranges::all_of(name, [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
    });
}

void initialize(const std::filesystem::path& root) noexcept
{
    g_root = root;

    g_stop = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_writer = g_stop != nullptr ? ::CreateThread(nullptr, 0, &writer_main, nullptr, 0, nullptr) : nullptr;

    // Without the thread, files are written when a script goes away and at shutdown - later, not never.
    if(g_writer == nullptr) {
        tw::lua::diag::note(-1, "tw.storage", "no writer thread - scripts' files are saved when a script is unloaded and when the game exits");
    }
}

void shutdown() noexcept
{
    if(g_writer != nullptr) {
        ::SetEvent(g_stop);
        ::WaitForSingleObject(g_writer, 5000);
        ::CloseHandle(g_writer);
        g_writer = nullptr;
    }
    if(g_stop != nullptr) {
        ::CloseHandle(g_stop);
        g_stop = nullptr;
    }

    flush_pass(-1, true);

    ::AcquireSRWLockExclusive(&g_lock);
    g_owners.clear();
    g_failed.clear();
    ::ReleaseSRWLockExclusive(&g_lock);
    g_any_failed.store(false, std::memory_order_relaxed);

    g_root.clear();
    g_tape.clear();
    g_pool.clear();
    g_text.clear();
}

bool read(int owner, std::string_view name, std::string_view& out, std::string& error) noexcept
{
    if(!valid_name(name)) {
        error = std::format("'{}' is not a storage name", name);
        return false;
    }

    ::AcquireSRWLockExclusive(&g_lock);
    owner_data* const data = touch(owner, error);
    file_entry* const file = data != nullptr ? find_file(*data, name) : nullptr;

    bool ok = file != nullptr && file->exists;
    if(data != nullptr && !ok) {
        error = std::format("there is no {}", name);
    }

    // The first read of what is on disk is the one system call of the read path.
    if(ok && !file->loaded) {
        std::string detail;
        ok = read_file(data->dir / std::filesystem::path { file->name }, file->bytes, detail);
        if(ok) {
            file->loaded = true;
            file->size = file->bytes.size();
        }
        else {
            file->bytes.clear();
            error = std::format("{} could not be read: {}", name, detail);
        }
    }

    if(ok) {
        out = file->bytes;
    }
    ::ReleaseSRWLockExclusive(&g_lock);
    return ok;
}

bool write(int owner, std::string_view name, std::string_view bytes, std::string& error) noexcept
{
    if(!valid_name(name)) {
        error = std::format("'{}' is not a storage name - letters, digits, '_', '.', '-', up to {}, not starting with '.'", name, k_max_name);
        return false;
    }
    if(bytes.size() > k_max_file) {
        error = std::format("{} would be {} bytes - a file holds up to {}", name, bytes.size(), k_max_file);
        return false;
    }

    ::AcquireSRWLockExclusive(&g_lock);
    owner_data* const data = touch(owner, error);
    bool ok = data != nullptr;

    if(ok) {
        file_entry* file = find_file(*data, name);
        int files = 0;
        std::size_t total = 0;
        for(const file_entry& entry : data->files) {
            if(entry.exists) {
                ++files;
                total += entry.size;
            }
        }

        const bool is_new = file == nullptr || !file->exists;
        const std::size_t old_size = is_new ? 0 : file->size;
        if(is_new && files >= k_max_files) {
            error = std::format("{} would be file {} - a script keeps up to {}", name, files + 1, k_max_files);
            ok = false;
        }
        else if(total - old_size + bytes.size() > k_max_total) {
            error = std::format("{} would bring the folder to {} bytes - a script keeps up to {}", name, total - old_size + bytes.size(), k_max_total);
            ok = false;
        }
        else {
            if(file == nullptr) {
                data->files.push_back({ std::string { name } });
                file = &data->files.back();
            }
            file->bytes.assign(bytes);
            file->size = bytes.size();
            file->loaded = true;
            file->exists = true;
            file->dirty = true;
        }
    }

    ::ReleaseSRWLockExclusive(&g_lock);
    return ok;
}

bool exists(int owner, std::string_view name) noexcept
{
    if(!valid_name(name)) {
        return false;
    }

    std::string error;
    ::AcquireSRWLockExclusive(&g_lock);
    owner_data* const data = touch(owner, error);
    const file_entry* const file = data != nullptr ? find_file(*data, name) : nullptr;
    const bool found = file != nullptr && file->exists;
    ::ReleaseSRWLockExclusive(&g_lock);
    return found;
}

bool remove(int owner, std::string_view name, std::string& error) noexcept
{
    if(!valid_name(name)) {
        error = std::format("'{}' is not a storage name", name);
        return false;
    }

    ::AcquireSRWLockExclusive(&g_lock);
    owner_data* const data = touch(owner, error);
    file_entry* const file = data != nullptr ? find_file(*data, name) : nullptr;
    const bool ok = file != nullptr && file->exists;
    if(ok) {
        file->bytes.clear();
        file->size = 0;
        file->loaded = true;
        file->exists = false;
        file->dirty = true;
    }
    else if(data != nullptr) {
        error = std::format("there is no {}", name);
    }
    ::ReleaseSRWLockExclusive(&g_lock);
    return ok;
}

std::vector<std::string> list(int owner) noexcept
{
    std::vector<std::string> names;
    std::string error;

    ::AcquireSRWLockExclusive(&g_lock);
    if(owner_data* const data = touch(owner, error); data != nullptr) {
        for(const file_entry& file : data->files) {
            if(file.exists) {
                names.push_back(file.name);
            }
        }
    }
    ::ReleaseSRWLockExclusive(&g_lock);

    std::ranges::sort(names);
    return names;
}

void drop_owner(int owner) noexcept
{
    if(owner < 0) {
        return;
    }

    flush_pass(owner, true);

    ::AcquireSRWLockExclusive(&g_lock);
    if(owner < static_cast<int>(g_owners.size())) {
        g_owners[static_cast<std::size_t>(owner)].reset();
    }
    ::ReleaseSRWLockExclusive(&g_lock);
}

void flush() noexcept
{
    flush_pass(-1, true);
}

void pump() noexcept
{
    if(!g_any_failed.load(std::memory_order_acquire)) [[likely]] {
        return;
    }

    std::vector<std::pair<int, std::string>> failed;
    ::AcquireSRWLockExclusive(&g_lock);
    failed.swap(g_failed);
    g_any_failed.store(false, std::memory_order_relaxed);
    ::ReleaseSRWLockExclusive(&g_lock);

    for(const auto& [owner, message] : failed) {
        tw::lua::diag::report(owner, tw::lua::diag::level::error, "tw.storage", message);
    }
}

bool json_to_tape(std::string_view text, std::vector<json_node>& tape, std::string& pool, std::string& error) noexcept
{
    tape.clear();
    pool.clear();

    // Lenient on what people type into a file by hand - a BOM, a comment, a trailing comma - and on nothing else.
    constexpr yyjson_read_flag k_flags = YYJSON_READ_ALLOW_BOM | YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS;
    yyjson_read_err read_error {};
    yyjson_doc* const doc = yyjson_read_opts(const_cast<char*>(text.data()), text.size(), k_flags, nullptr, &read_error);
    if(doc == nullptr) {
        error = std::format("it is not JSON: {} at byte {}", read_error.msg != nullptr ? read_error.msg : "?", read_error.pos);
        return false;
    }

    const bool ok = push_tape(yyjson_doc_get_root(doc), 0, tape, pool, error);
    yyjson_doc_free(doc);
    if(!ok) {
        tape.clear();
        pool.clear();
        return false;
    }

    for(json_node& node : tape) {
        if(node.type == static_cast<int>(json_type::string)) {
            node.str = pool.data() + node.count;
            node.count = 0;
        }
    }
    return true;
}

bool tape_to_json(std::span<const json_node> tape, std::string& out, std::string& error) noexcept
{
    yyjson_mut_doc* const doc = yyjson_mut_doc_new(nullptr);
    if(doc == nullptr) {
        error = "out of memory";
        return false;
    }

    std::size_t at = 0;
    yyjson_mut_val* const root = build_value(doc, tape, at, 0, error);
    bool ok = root != nullptr;
    if(ok && at != tape.size()) {
        error = "the value is followed by more";
        ok = false;
    }

    if(ok) {
        yyjson_mut_doc_set_root(doc, root);
        yyjson_write_err write_error {};
        std::size_t length = 0;
        char* const text = yyjson_mut_write_opts(doc, YYJSON_WRITE_PRETTY_TWO_SPACES | YYJSON_WRITE_NEWLINE_AT_END, nullptr, &length, &write_error);
        if(text == nullptr) {
            error = write_error.code == YYJSON_WRITE_ERROR_INVALID_STRING ? std::string { "a string is not valid UTF-8" }
                                                                           : std::format("it cannot be written as JSON: {}", write_error.msg != nullptr ? write_error.msg : "?");
            ok = false;
        }
        else {
            out.assign(text, length);
            std::free(text);
        }
    }

    yyjson_mut_doc_free(doc);
    return ok;
}

bool read_json(int owner, std::string_view name, std::span<const json_node>& tape, std::string& error) noexcept
{
    std::string_view text;
    if(!read(owner, name, text, error)) {
        return false;
    }

    std::string detail;
    if(!json_to_tape(text, g_tape, g_pool, detail)) {
        error = std::format("{}: {}", name, detail);
        return false;
    }

    tape = g_tape;
    return true;
}

bool write_json(int owner, std::string_view name, std::span<const json_node> tape, std::string& error) noexcept
{
    std::string detail;
    if(!tape_to_json(tape, g_text, detail)) {
        error = std::format("{} was not written: {}", name, detail);
        return false;
    }

    return write(owner, name, g_text, error);
}
} // namespace tw::lua::storage
