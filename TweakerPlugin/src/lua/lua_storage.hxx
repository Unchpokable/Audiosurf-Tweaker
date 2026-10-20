#pragma once

// A script's own files: tw.storage (Docs/Internal/script-tabs.md §3.8).
//
// One folder per script, TweakerStuff\ScriptData\<file without .lua>\ - by file name, like scripts.cfg, so a
// Reload, a toggle and a twllink rebuild under the same name see the same folder. Names in it are flat:
// valid_name() is the one place that decides what a script may name, and nothing it accepts can climb out of
// the folder. Limits keep honest mistakes small: a file, the number of files, the folder's total.
//
// **No system call on the write path.** A script may write from on_frame, every frame; a write puts the bytes
// into memory - the newest version of a name wins - and returns. A thread of this module writes what changed to
// disk every 100 ms, each file through a temporary one and MoveFileEx, so a game that dies mid-write leaves the
// old file or the new one, never half of either. A script going away (lua_sched::unload) and shutdown write its
// files there and then.
//
// **Reads** come from that same memory: the first read of a file that is on disk reads it - a system call, which
// is why scripts are told to read while they load - and every read after that, and every read of a name the
// script wrote, is a copy. The first use of the folder at all lists it, once, for the limits.
//
// JSON goes through yyjson both ways. To Lua it is a **tape**: the values in pre-order, one json_node each, which
// the prelude turns into tables - the FFI cannot hand C values onto the Lua stack, and this needs no
// lua_CFunction. From Lua it is the same tape the other way.
//
// Calls: the engine/render thread. The writer thread only ever touches what it takes under the lock.
namespace tw::lua::storage
{
inline constexpr std::size_t k_max_name = 64;
inline constexpr std::size_t k_max_file = 1u << 20;  // 1 MiB
inline constexpr int k_max_files = 64;
inline constexpr std::size_t k_max_total = 8u << 20; // 8 MiB, the folder
inline constexpr int k_max_depth = 64;               // JSON nesting, both ways

// [A-Za-z0-9_.-]{1,64}, not starting with a dot: no separator, no drive, no "..", nothing hidden.
[[nodiscard]] bool valid_name(std::string_view name) noexcept;

// `root` is TweakerStuff\ScriptData. Starts the writer thread.
void initialize(const std::filesystem::path& root) noexcept;

// Stops the writer and writes everything still waiting.
void shutdown() noexcept;

// Every call says why it failed in `error`, a short reason the prelude hands the script as its second result.
//
// `out` stays valid until the next call into this module.
bool read(int owner, std::string_view name, std::string_view& out, std::string& error) noexcept;
bool write(int owner, std::string_view name, std::string_view bytes, std::string& error) noexcept;
[[nodiscard]] bool exists(int owner, std::string_view name) noexcept;
bool remove(int owner, std::string_view name, std::string& error) noexcept;

// The script's files, sorted.
[[nodiscard]] std::vector<std::string> list(int owner) noexcept;

// What the script wrote goes to disk now, and the module forgets the script - a reloaded one reads its files
// afresh, including any the user edited meanwhile. lua_sched::unload.
void drop_owner(int owner) noexcept;

// Writes everything waiting, now, on the calling thread.
void flush() noexcept;

// Once per overlay frame: what the writer thread could not write goes to the scripts' diagnostics.
void pump() noexcept;

// --- JSON ------------------------------------------------------------------------------------------------------

enum class json_type : int {
    null = 0,
    false_value = 1,
    true_value = 2,
    number = 3,
    string = 4, // also an object's key
    array = 5,  // `count` children follow
    object = 6, // `count` pairs follow: a string node, then the value
};

// One value of the tape. Layout is ABI - the prelude declares the same struct.
struct json_node {
    int type;
    int count;
    double number;
    const char* str;
    int len;
};

// Text to tape. Strings point into `pool`; both stay valid until they are changed.
bool json_to_tape(std::string_view text, std::vector<json_node>& tape, std::string& pool, std::string& error) noexcept;

// Tape to text: two-space indented, a newline at the end. Refuses NaN and infinity, strings that are not
// UTF-8, and a tape that does not add up.
bool tape_to_json(std::span<const json_node> tape, std::string& out, std::string& error) noexcept;

// What the read path of tw.storage.read_json hands the prelude: the file through json_to_tape, kept here until
// the next call.
bool read_json(int owner, std::string_view name, std::span<const json_node>& tape, std::string& error) noexcept;
bool write_json(int owner, std::string_view name, std::span<const json_node> tape, std::string& error) noexcept;
} // namespace tw::lua::storage
