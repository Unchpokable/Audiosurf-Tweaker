#include "pch.hxx"

#include "lua/lua_diag.hxx"

#include "engine/engine_state.hxx"

#include "lua/lua_registry.hxx"
#include "lua/lua_sched.hxx"

#include "plugin/diagnostics.hxx"

#include "ui/plugins/static/notefeed.hxx"

namespace
{
using tw::lua::diag::entry;
using tw::lua::diag::level;

// Per script, not in total: a script that says many different things is one the author has to read
// anyway, and the oldest low-level record is what goes when it runs out.
constexpr std::size_t k_max_entries = 24;

// The notefeed allowance, per script: this many pushes in any rolling minute. Errors are already one
// push per record, so reaching this takes either many different errors or a script calling
// tw.notify in a loop - both of which the tab shows better than a column of toasts.
constexpr int k_notefeed_per_minute = 5;
constexpr std::uint64_t k_minute_ms = 60'000;

// How long a tw.pending keeps a script "waiting" after it was last said, in engine frames. A couple
// of seconds at a normal rate: long enough that a script saying it from on_tick every frame never
// flickers out, short enough that one that stopped saying it stops looking stuck.
constexpr std::uint32_t k_pending_fresh_frames = 120;

struct owner_log {
    std::vector<entry> entries;

    // When the last k_notefeed_per_minute pushes happened, as a ring. The slot the head points at is
    // the oldest: if it is still inside the minute, the allowance is spent.
    std::array<std::uint64_t, k_notefeed_per_minute> pushes {};
    int push_head = 0;
    bool cap_said = false;
};

// Indexed by owner + 1, so the layer's own records (owner -1) sit at 0. Script ids are dense and
// start at 0 (lua_registry), so this stays as long as the script list.
std::vector<owner_log> g_logs;

// Some error was filed before the game was up and has not been said yet.
bool g_held = false;

owner_log* log_of(int owner, bool create)
{
    const std::size_t index = static_cast<std::size_t>(std::max(owner, -1) + 1);
    if(index >= g_logs.size()) {
        if(!create) {
            return nullptr;
        }
        g_logs.resize(index + 1);
    }

    return &g_logs[index];
}

std::string_view first_line(std::string_view message) noexcept
{
    const std::size_t end = message.find_first_of("\r\n");
    return end == std::string_view::npos ? message : message.substr(0, end);
}

bool is_digit(char c) noexcept
{
    return c >= '0' && c <= '9';
}

bool is_hex(char c) noexcept
{
    return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

// The dedup key. First line only - a traceback's frames are the same mistake seen from further away.
//
// A leading `chunk:line:` is kept as it is: that is Lua's own position prefix on a runtime error, and
// masking it would fold two different mistakes in one script into one record. Everything after it has
// each run of digits - and each 0x-prefixed hex number - replaced by `#`, so a message that quotes a
// value does not become a new record every time the value changes.
std::string shape_of(std::string_view message)
{
    message = first_line(message);

    std::string out;
    out.reserve(message.size());

    std::size_t i = 0;
    if(const std::size_t colon = message.find(':'); colon != std::string_view::npos) {
        std::size_t j = colon + 1;
        while(j < message.size() && is_digit(message[j])) {
            ++j;
        }
        if(j > colon + 1 && j < message.size() && message[j] == ':') {
            out.assign(message.substr(0, j + 1));
            i = j + 1;
        }
    }

    while(i < message.size()) {
        const char c = message[i];

        if(c == '0' && i + 2 < message.size() && (message[i + 1] == 'x' || message[i + 1] == 'X') && is_hex(message[i + 2])) {
            i += 2;
            while(i < message.size() && is_hex(message[i])) {
                ++i;
            }
            out.push_back('#');
            continue;
        }

        if(is_digit(c)) {
            while(i < message.size() && is_digit(message[i])) {
                ++i;
            }
            out.push_back('#');
            continue;
        }

        out.push_back(c);
        ++i;
    }

    return out;
}

// "Lua: <script>: <text>" - the script's display name, because the file name is what the author
// knows and the header name is what the player saw in the tab.
std::string feed_line(int owner, std::string_view text)
{
    if(const tw::lua::script* script = tw::lua::registry::find(owner); script != nullptr) {
        return std::format("Lua: {}: {}", script->name, text);
    }

    return std::format("Lua: {}", text);
}

// Into the notefeed if the script still has allowance this minute. The first push past the cap says
// so, once per spent window, so a script that fell silent is not a mystery.
bool push_capped(owner_log& log, int owner, std::string_view text)
{
    if(owner < 0) {
        tw::ui::plugins::statics::notefeed::push(feed_line(owner, text));
        return true;
    }

    const std::uint64_t now = ::GetTickCount64();
    std::uint64_t& oldest = log.pushes[static_cast<std::size_t>(log.push_head)];

    if(oldest != 0 && now - oldest < k_minute_ms) {
        if(!log.cap_said) {
            log.cap_said = true;
            tw::ui::plugins::statics::notefeed::push(feed_line(owner, "more messages than the notefeed takes - the rest are in the Scripts tab"));
        }
        return false;
    }

    oldest = now;
    log.push_head = (log.push_head + 1) % k_notefeed_per_minute;
    log.cap_said = false;

    tw::ui::plugins::statics::notefeed::push(feed_line(owner, text));
    return true;
}

void log_first(int owner, level severity, std::string_view where, std::string_view message)
{
    const tw::lua::script* script = tw::lua::registry::find(owner);
    const std::string_view who = script != nullptr ? std::string_view { script->file } : std::string_view { "layer" };

    switch(severity) {
    case level::pending:
    case level::info:
        TW_LOG_INFO("lua [{}] {}: {}", who, where, message);
        break;
    case level::warn:
        TW_LOG_WARNING("lua [{}] {}: {}", who, where, message);
        break;
    case level::error:
    case level::fatal:
        TW_LOG_ERROR("lua [{}] {}: {}", who, where, message);
        break;
    }
}

// The routing table in the header, for a record's first occurrence (and every fatal one).
void route(owner_log& log, int owner, entry& record)
{
    switch(record.severity) {
    case level::pending:
    case level::info:
    case level::warn:
        return;

    case level::error:
        // Never while the game is still coming up - nothing is supposed to be running then, and the
        // one thing a player should see during loading is the loading screen. Held, not dropped.
        if(!tw::engine::state::ready()) {
            g_held = true;
            return;
        }
        record.announced = true;
        (void)push_capped(log, owner, record.text);
        return;

    case level::fatal:
        record.announced = true;
        tw::ui::plugins::statics::notefeed::push(feed_line(owner, record.text));
        return;
    }
}

// Makes room for one more record: the least severe goes first, and among equals the one seen longest
// ago. A fatal record is only ever evicted by another fatal one.
void evict(owner_log& log)
{
    auto victim = log.entries.end();
    for(auto it = log.entries.begin(); it != log.entries.end(); ++it) {
        if(victim == log.entries.end() || it->severity < victim->severity
            || (it->severity == victim->severity && it->last_frame < victim->last_frame)) {
            victim = it;
        }
    }

    if(victim != log.entries.end()) {
        log.entries.erase(victim);
    }
}
} // namespace

namespace tw::lua::diag
{
void report(int owner, level severity, std::string_view where, std::string_view message) noexcept
{
    owner_log* const log = log_of(owner, true);
    const std::string shape = shape_of(message);
    const std::uint32_t now = tw::lua::sched::frame();

    for(entry& record : log->entries) {
        if(record.severity != severity || record.shape != shape || record.where != where) {
            continue;
        }

        ++record.count;
        record.last_frame = now;
        record.text.assign(first_line(message));

        // A fatal record is a suspension, and each one is news: the player pressed Resume, and it
        // happened again.
        if(severity == level::fatal) {
            log_first(owner, severity, where, message);
            route(*log, owner, record);
        }
        return;
    }

    if(log->entries.size() >= k_max_entries) {
        evict(*log);
    }

    entry record;
    record.severity = severity;
    record.where.assign(where);
    record.shape = shape;
    record.text.assign(first_line(message));
    record.detail.assign(message);
    record.count = 1;
    record.first_frame = now;
    record.last_frame = now;

    log->entries.push_back(std::move(record));

    log_first(owner, severity, where, message);
    route(*log, owner, log->entries.back());
}

void notify(int owner, std::string_view message) noexcept
{
    owner_log* const log = log_of(owner, true);
    if(push_capped(*log, owner, message)) {
        return;
    }

    report(owner, level::info, "tw.notify", message);
}

void announce_held() noexcept
{
    if(!g_held) [[likely]] {
        return;
    }

    g_held = false;

    for(std::size_t index = 0; index < g_logs.size(); ++index) {
        const int owner = static_cast<int>(index) - 1;
        owner_log& log = g_logs[index];

        for(entry& record : log.entries) {
            if(record.severity == level::error && !record.announced) {
                record.announced = true;
                (void)push_capped(log, owner, record.text);
            }
        }
    }
}

void clear(int owner) noexcept
{
    if(owner_log* const log = log_of(owner, false); log != nullptr) {
        *log = owner_log {};
    }
}

void clear_all() noexcept
{
    g_logs.clear();
    g_held = false;
}

std::span<const entry> entries(int owner) noexcept
{
    const owner_log* const log = log_of(owner, false);
    if(log == nullptr) {
        return {};
    }

    return log->entries;
}

bool waiting(int owner) noexcept
{
    const owner_log* const log = log_of(owner, false);
    if(log == nullptr) {
        return false;
    }

    const std::uint32_t now = tw::lua::sched::frame();
    for(const entry& record : log->entries) {
        if(record.severity == level::pending && now - record.last_frame <= k_pending_fresh_frames) {
            return true;
        }
    }

    return false;
}

const char* name(level severity) noexcept
{
    switch(severity) {
    case level::pending:
        return "waiting";
    case level::info:
        return "info";
    case level::warn:
        return "warning";
    case level::error:
        return "error";
    case level::fatal:
        return "suspended";
    }

    return "";
}
} // namespace tw::lua::diag
