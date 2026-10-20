#include "pch.hxx"

#include "lua/lua_log.hxx"

#include "lua/lua_registry.hxx"
#include "lua/lua_sched.hxx"

namespace
{
// The queue between the callers and the file. 64 KiB is a few hundred lines - several seconds of a script
// failing in a new way every frame, which the error budget suspends long before - and a power of two, so a
// position is a mask away from an index.
constexpr std::size_t k_ring_bytes = 64 * 1024;

// One entry's text, prefix and traceback included. Formatted on the caller's stack before the lock is taken.
// A traceback twenty frames deep is about 2 KiB; past this it is cut, and says so.
constexpr std::size_t k_record_max = 8 * 1024;

// A session's file stops growing here, with a line saying why. Several hours of an author's edit-reload loop
// stay far below it; a script that found a way to say something new every frame does not.
constexpr std::uint64_t k_session_limit = 4 * 1024 * 1024;

// Sessions kept in Logs\, this one included. A session is usually a few kilobytes, so twenty is a few
// days of play or an evening of an author's restarts - and at worst 80 MiB, if every one of them hit the limit.
constexpr std::size_t k_keep_sessions = 20;

// How often the writer thread wakes. Also the most the log can be behind the game - see flush() for the
// moments where that matters.
constexpr DWORD k_writer_period_ms = 100;

// Under the first line of an entry: its traceback, a multi-line message.
constexpr std::string_view k_indent = "    ";

// length, then the QueryPerformanceCounter value of the moment write() was called, then the text.
constexpr std::size_t k_header_bytes = sizeof(std::uint32_t) + sizeof(std::int64_t);

// --- the caller side: the ring and its lock ---------------------------------------------------------------

std::atomic<bool> g_open = false;

// Held by write() for one copy into the ring and by drain() for one copy out of it - never across a system
// call, so spinning on it is cheaper than anything the kernel could offer.
std::atomic_flag g_ring_lock;

std::array<char, k_ring_bytes> g_ring;
std::uint64_t g_head = 0; // bytes ever written; under g_ring_lock
std::uint64_t g_tail = 0; // bytes ever taken out; under g_ring_lock
std::uint32_t g_dropped = 0;

void lock_ring() noexcept
{
    while(g_ring_lock.test_and_set(std::memory_order_acquire)) {
        YieldProcessor();
    }
}

void unlock_ring() noexcept
{
    g_ring_lock.clear(std::memory_order_release);
}

void ring_put(const void* data, std::size_t size) noexcept
{
    const std::size_t at = static_cast<std::size_t>(g_head) & (k_ring_bytes - 1);
    const std::size_t first = std::min(size, k_ring_bytes - at);
    std::memcpy(g_ring.data() + at, data, first);
    std::memcpy(g_ring.data(), static_cast<const char*>(data) + first, size - first);
    g_head += size;
}

[[nodiscard]] std::int64_t now_ticks() noexcept
{
    LARGE_INTEGER now {};
    ::QueryPerformanceCounter(&now);
    return now.QuadPart;
}

// --- the file side: everything below runs under g_file_lock -----------------------------------------------

SRWLOCK g_file_lock = SRWLOCK_INIT;
HANDLE g_file = INVALID_HANDLE_VALUE;
std::filesystem::path g_path;
HANDLE g_stop = nullptr;
HANDLE g_writer = nullptr;

// What QueryPerformanceCounter read when the session opened, and the wall clock at that moment: a line's time
// is worked out from its tick count on the writer's side, so the caller only ever reads the counter.
std::int64_t g_base_ticks = 0;
std::int64_t g_frequency = 1;
std::uint64_t g_base_time = 0; // FILETIME, UTC

std::uint64_t g_written = 0;
bool g_full = false;

std::array<char, k_ring_bytes> g_drained;
std::array<char, 32 * 1024> g_out;
std::size_t g_out_used = 0;

void write_out() noexcept
{
    if(g_out_used == 0) {
        return;
    }

    DWORD written = 0;
    ::WriteFile(g_file, g_out.data(), static_cast<DWORD>(g_out_used), &written, nullptr);
    g_written += g_out_used;
    g_out_used = 0;
}

void append(std::string_view bytes) noexcept
{
    if(g_out_used + bytes.size() > g_out.size()) {
        write_out();
    }

    const std::size_t size = std::min(bytes.size(), g_out.size());
    std::memcpy(g_out.data() + g_out_used, bytes.data(), size);
    g_out_used += size;
}

// "21:04:11.120 " for a tick count, in local time.
std::string_view stamp(std::int64_t ticks, std::array<char, 32>& buffer) noexcept
{
    const std::int64_t delta = std::max<std::int64_t>(ticks - g_base_ticks, 0);
    const std::uint64_t hundreds_ns = static_cast<std::uint64_t>(delta / g_frequency) * 10'000'000ull
        + static_cast<std::uint64_t>(delta % g_frequency) * 10'000'000ull / static_cast<std::uint64_t>(g_frequency);
    const std::uint64_t time = g_base_time + hundreds_ns;

    FILETIME utc_file {};
    utc_file.dwLowDateTime = static_cast<DWORD>(time);
    utc_file.dwHighDateTime = static_cast<DWORD>(time >> 32);

    SYSTEMTIME utc {};
    SYSTEMTIME local {};
    if(::FileTimeToSystemTime(&utc_file, &utc) == FALSE || ::SystemTimeToTzSpecificLocalTime(nullptr, &utc, &local) == FALSE) {
        return "--:--:--.--- ";
    }

    const auto result =
        std::format_to_n(buffer.data(), buffer.size(), "{:02}:{:02}:{:02}.{:03} ", local.wHour, local.wMinute, local.wSecond, local.wMilliseconds);
    return { buffer.data(), static_cast<std::size_t>(result.out - buffer.data()) };
}

// One line into the file - or, the first time the session would go over its limit, the line saying it has.
void emit(std::int64_t ticks, std::string_view body) noexcept
{
    if(g_full) {
        return;
    }

    std::array<char, 32> time;
    const std::string_view when = stamp(ticks, time);

    // Room for the notice is kept back, so the file that stops says so.
    constexpr std::uint64_t k_notice_room = 256;
    if(g_written + g_out_used + when.size() + body.size() + 2 > k_session_limit - k_notice_room) {
        g_full = true;
        append(when);
        append("f-       limit     (layer): this session's log is full - nothing more is written until the game is restarted\r\n");
        return;
    }

    append(when);
    append(body);
    append("\r\n");
}

// Takes everything out of the ring and writes it. The ring lock is held for the copy, the file lock for all of
// it - the writer thread and flush() are the two callers.
void drain() noexcept
{
    ::AcquireSRWLockExclusive(&g_file_lock);

    if(g_file == INVALID_HANDLE_VALUE) {
        ::ReleaseSRWLockExclusive(&g_file_lock);
        return;
    }

    lock_ring();
    const std::size_t size = static_cast<std::size_t>(g_head - g_tail);
    const std::size_t at = static_cast<std::size_t>(g_tail) & (k_ring_bytes - 1);
    const std::size_t first = std::min(size, k_ring_bytes - at);
    std::memcpy(g_drained.data(), g_ring.data() + at, first);
    std::memcpy(g_drained.data() + first, g_ring.data(), size - first);
    g_tail = g_head;
    const std::uint32_t dropped = g_dropped;
    g_dropped = 0;
    unlock_ring();

    // Lost lines are said where they would have been - before whatever made it into the ring after them.
    if(dropped != 0) {
        std::array<char, 160> notice;
        const auto result = std::format_to_n(notice.data(),
            notice.size(),
            "f-       dropped   (layer): {} line(s) lost - more than the log takes in {} ms",
            dropped,
            k_writer_period_ms);
        emit(now_ticks(), { notice.data(), static_cast<std::size_t>(result.out - notice.data()) });
    }

    std::size_t offset = 0;
    while(offset + k_header_bytes <= size) {
        std::uint32_t length = 0;
        std::int64_t ticks = 0;
        std::memcpy(&length, g_drained.data() + offset, sizeof(length));
        std::memcpy(&ticks, g_drained.data() + offset + sizeof(length), sizeof(ticks));
        offset += k_header_bytes;

        emit(ticks, { g_drained.data() + offset, length });
        offset += length;
    }

    write_out();

    ::ReleaseSRWLockExclusive(&g_file_lock);
}

DWORD WINAPI writer_main(void*)
{
    while(::WaitForSingleObject(g_stop, k_writer_period_ms) == WAIT_TIMEOUT) {
        drain();
    }

    return 0;
}

// Copies `text` after `used` bytes of `out`, indenting every line after the first and dropping carriage
// returns and trailing line breaks. Returns the new length; a text that does not fit ends in "...".
std::size_t copy_text(std::array<char, k_record_max>& out, std::size_t used, std::string_view text) noexcept
{
    while(!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
        text.remove_suffix(1);
    }

    constexpr std::string_view k_cut = " ...";
    const std::size_t limit = out.size() - k_cut.size();

    for(const char c : text) {
        if(c == '\r') {
            continue;
        }

        const std::size_t need = c == '\n' ? 2 + k_indent.size() : 1;
        if(used + need > limit) {
            std::memcpy(out.data() + used, k_cut.data(), k_cut.size());
            return used + k_cut.size();
        }

        if(c == '\n') {
            out[used++] = '\r';
            out[used++] = '\n';
            std::memcpy(out.data() + used, k_indent.data(), k_indent.size());
            used += k_indent.size();
        }
        else {
            out[used++] = c;
        }
    }

    return used;
}

bool is_digit(wchar_t c) noexcept
{
    return c >= L'0' && c <= L'9';
}

// Whether `name` is one of ours: `scripts-YYYY-MM-DD_hh-mm-ss.log`, or with `_N` before the extension when two
// sessions started in the same second. Only these are ever deleted - `scripts-for-the-issue.log` is the user's.
bool is_session_name(std::wstring_view name) noexcept
{
    constexpr std::wstring_view k_shape = L"scripts-0000-00-00_00-00-00";
    if(name.size() < k_shape.size() + 4 || !name.ends_with(L".log")) {
        return false;
    }

    for(std::size_t i = 0; i < k_shape.size(); ++i) {
        if(k_shape[i] == L'0' ? !is_digit(name[i]) : name[i] != k_shape[i]) {
            return false;
        }
    }

    std::wstring_view rest = name.substr(k_shape.size(), name.size() - k_shape.size() - 4);
    if(rest.empty()) {
        return true;
    }
    if(rest.size() < 2 || rest[0] != L'_') {
        return false;
    }

    return std::ranges::all_of(rest.substr(1), is_digit);
}

// Deletes the oldest session files until `keep` are left. Names sort in time order, so oldest is first by name.
// A file that cannot be deleted - open in an editor without FILE_SHARE_DELETE - stays, and is tried next time.
void prune(const std::filesystem::path& logs_dir, std::size_t keep) noexcept
{
    std::vector<std::wstring> sessions;

    WIN32_FIND_DATAW found {};
    const HANDLE search = ::FindFirstFileW((logs_dir / L"scripts-*.log").c_str(), &found);
    if(search == INVALID_HANDLE_VALUE) {
        return;
    }
    do {
        if((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 && is_session_name(found.cFileName)) {
            sessions.emplace_back(found.cFileName);
        }
    } while(::FindNextFileW(search, &found) != FALSE);
    ::FindClose(search);

    if(sessions.size() <= keep) {
        return;
    }

    std::ranges::sort(sessions);
    for(std::size_t i = 0; i + keep < sessions.size(); ++i) {
        ::DeleteFileW((logs_dir / sessions[i]).c_str());
    }
}

// Creates this session's file under a name no earlier session has: CREATE_NEW, so a name that exists - a second
// launch within the same second - moves on to `_2`, `_3`, rather than writing into somebody else's log.
HANDLE create_session_file(const std::filesystem::path& logs_dir, const SYSTEMTIME& local, std::filesystem::path& path) noexcept
{
    // Readable and deletable while the game has it open: an editor or `tail -f` next to a running game is
    // exactly the use.
    constexpr DWORD k_share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;

    const std::wstring stem = std::format(L"scripts-{:04}-{:02}-{:02}_{:02}-{:02}-{:02}",
        local.wYear,
        local.wMonth,
        local.wDay,
        local.wHour,
        local.wMinute,
        local.wSecond);

    for(int attempt = 1; attempt <= 99; ++attempt) {
        path = logs_dir / (attempt == 1 ? stem + L".log" : std::format(L"{}_{}.log", stem, attempt));

        const HANDLE file = ::CreateFileW(path.c_str(), GENERIC_WRITE, k_share, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if(file != INVALID_HANDLE_VALUE || ::GetLastError() != ERROR_FILE_EXISTS) {
            if(file == INVALID_HANDLE_VALUE) {
                path.clear();
            }
            return file;
        }
    }

    path.clear();
    return INVALID_HANDLE_VALUE;
}
} // namespace

namespace tw::lua::log
{
bool open(const std::filesystem::path& logs_dir, std::string_view header) noexcept
{
    if(g_open.load(std::memory_order_acquire) || logs_dir.empty()) {
        return g_open.load(std::memory_order_acquire);
    }

    ::CreateDirectoryW(logs_dir.parent_path().c_str(), nullptr);
    ::CreateDirectoryW(logs_dir.c_str(), nullptr);

    SYSTEMTIME local {};
    ::GetLocalTime(&local);

    // Room for this session first, then the file: the newest k_keep_sessions - 1 stay, this one makes the count.
    prune(logs_dir, k_keep_sessions - 1);

    std::filesystem::path path;
    const HANDLE file = create_session_file(logs_dir, local, path);
    if(file == INVALID_HANDLE_VALUE) {
        return false;
    }

    LARGE_INTEGER frequency {};
    ::QueryPerformanceFrequency(&frequency);
    g_frequency = frequency.QuadPart > 0 ? frequency.QuadPart : 1;
    g_base_ticks = now_ticks();

    FILETIME base {};
    ::GetSystemTimePreciseAsFileTime(&base);
    g_base_time = (static_cast<std::uint64_t>(base.dwHighDateTime) << 32) | base.dwLowDateTime;

    ::AcquireSRWLockExclusive(&g_file_lock);
    g_file = file;
    g_path = std::move(path);
    g_written = 0;
    g_full = false;
    g_out_used = 0;

    std::array<char, 512> line;
    const auto result = std::format_to_n(line.data(),
        line.size() - 2,
        "===== {:04}-{:02}-{:02} {:02}:{:02}:{:02} {} =====",
        local.wYear,
        local.wMonth,
        local.wDay,
        local.wHour,
        local.wMinute,
        local.wSecond,
        header);
    std::size_t length = std::min(static_cast<std::size_t>(result.out - line.data()), line.size() - 2);
    line[length++] = '\r';
    line[length++] = '\n';
    append({ line.data(), length });
    append("time         frame    kind      script: where: text\r\n");
    write_out();
    ::ReleaseSRWLockExclusive(&g_file_lock);

    lock_ring();
    g_head = 0;
    g_tail = 0;
    g_dropped = 0;
    unlock_ring();

    g_open.store(true, std::memory_order_release);

    g_stop = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_writer = g_stop != nullptr ? ::CreateThread(nullptr, 0, &writer_main, nullptr, 0, nullptr) : nullptr;

    // No writer means the log is written only at the cold-path flushes, and anything a frame says between
    // them may be dropped. Worth one line, not a failure.
    if(g_writer == nullptr) {
        write(-1, "info", {}, "no writer thread - lines are written only when scripts are loaded, switched or reloaded");
        flush();
    }

    return true;
}

void close() noexcept
{
    if(!g_open.exchange(false, std::memory_order_acq_rel)) {
        return;
    }

    if(g_writer != nullptr) {
        ::SetEvent(g_stop);
        ::WaitForSingleObject(g_writer, 2000);
        ::CloseHandle(g_writer);
        g_writer = nullptr;
    }
    if(g_stop != nullptr) {
        ::CloseHandle(g_stop);
        g_stop = nullptr;
    }

    drain();

    ::AcquireSRWLockExclusive(&g_file_lock);
    ::CloseHandle(g_file);
    g_file = INVALID_HANDLE_VALUE;
    g_path.clear();
    ::ReleaseSRWLockExclusive(&g_file_lock);
}

void write(int owner, std::string_view kind, std::string_view where, std::string_view text) noexcept
{
    if(!g_open.load(std::memory_order_acquire)) [[likely]] {
        return;
    }

    const std::int64_t ticks = now_ticks();

    const tw::lua::script* const script = tw::lua::registry::find(owner);
    const std::string_view who = script != nullptr ? std::string_view { script->file } : std::string_view { "(layer)" };

    // The prefix, then the text: "f4711    error     hud.lua: on_frame: hud.lua:88: attempt to ...". A place
    // that only repeats the kind ("suspended: suspended - ...") is left out.
    std::array<char, k_record_max> body;
    const bool has_where = !where.empty() && where != kind;
    const auto prefix = has_where
        ? std::format_to_n(body.data(), 192, "f{:<7} {:<9} {}: {}: ", tw::lua::sched::frame(), kind, who, where)
        : std::format_to_n(body.data(), 192, "f{:<7} {:<9} {}: ", tw::lua::sched::frame(), kind, who);
    const std::size_t used = copy_text(body, std::min(static_cast<std::size_t>(prefix.size), std::size_t { 192 }), text);

    lock_ring();
    if(k_ring_bytes - static_cast<std::size_t>(g_head - g_tail) < k_header_bytes + used) [[unlikely]] {
        ++g_dropped;
        unlock_ring();
        return;
    }

    const auto length = static_cast<std::uint32_t>(used);
    ring_put(&length, sizeof(length));
    ring_put(&ticks, sizeof(ticks));
    ring_put(body.data(), used);
    unlock_ring();
}

void event(int owner, std::string_view text) noexcept
{
    write(owner, "script", {}, text);
}

void flush() noexcept
{
    if(!g_open.load(std::memory_order_acquire)) {
        return;
    }

    drain();
}

bool is_open() noexcept
{
    return g_open.load(std::memory_order_acquire);
}

const std::filesystem::path& path() noexcept
{
    return g_path;
}
} // namespace tw::lua::log
