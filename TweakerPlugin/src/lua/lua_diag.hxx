#pragma once

// Everything the scripting layer has to say about a script, in one deduplicating sink
// (lua-engine-fix-roadmap.md §7).
//
// **The problem this solves is volume, not wording.** A script that fails every frame used to be one
// of two things: a notefeed full of the same line, or - after the one-strike latch - silence for
// every script in the layer. Neither tells the player which script, and neither tells the author how
// often. So every message becomes a *record*, keyed by (script, where, shape), and a repeat bumps a
// counter on the record instead of producing another line. "Shape" is the message with its numbers
// masked, so "row 1234" and "row 1235" are one record, not two - except for the leading `file:line:`
// Lua puts on a runtime error, which is kept exact, because two different lines are two different
// mistakes.
//
// Where each level goes:
//
// | level   | log              | notefeed                                | Scripts tab            |
// |---------|------------------|-----------------------------------------|------------------------|
// | pending | first time       | never                                   | "waiting", while fresh |
// | info    | first time       | never                                   | yes                    |
// | warn    | first time       | never                                   | yes, with the count    |
// | error   | first time       | first time per record, and only once    | yes, with the count    |
// |         |                  | the game is up - earlier ones are held  |                        |
// |         |                  | and said on the first ready frame       |                        |
// | fatal   | every time       | every time (it is rare by construction) | yes, highlighted       |
//
// On top of that, one cap for everything a script can put into the notefeed - its errors and its own
// tw.notify calls together: a handful per minute, then one line saying the rest went to the tab.
// That is the guarantee against the case nobody thought of.
//
// Owner -1 is the layer itself (a refused write carries no script; the dispatchers' own failures are
// nobody's script). Engine/render thread only, like the rest of src/lua/.
namespace tw::lua::diag
{
// Numbering is shared with the prelude, which passes pending..error through tw_diag. fatal is the
// scheduler's alone - a script cannot say it about itself.
enum class level : int {
    pending = 0,
    info = 1,
    warn = 2,
    error = 3,
    fatal = 4,
};

struct entry {
    level severity = level::info;
    std::string where;  // a callback ("on_frame"), an API ("tw.on_call") or a place ("hud.lua:88")
    std::string shape;  // the dedup key: the message with its numbers masked
    std::string text;   // the latest occurrence, first line only - what the tab shows
    std::string detail; // the first occurrence in full, traceback included - what the tooltip shows
    std::uint32_t count = 0;
    std::uint32_t first_frame = 0;
    std::uint32_t last_frame = 0;
    bool announced = false; // an error that has been put into the notefeed
};

// Files one message. Cheap on a repeat (a compare over the script's handful of records and a counter
// bump), so it is safe from a callback that fails every frame - which is precisely when it is used.
void report(int owner, level severity, std::string_view where, std::string_view message) noexcept;

// tw.notify on behalf of a script: straight into the notefeed while under the cap, into the tab as an
// info record once over it. Owner -1 is not capped.
void notify(int owner, std::string_view message) noexcept;

// Errors that happened before the game was up have been held back rather than dropped; this says
// them. Called by the scheduler on ready frames - a no-op after the first unless something new is
// held, so asking every frame costs one branch.
void announce_held() noexcept;

// Forgets one script's records and its notefeed allowance - a script that is switched off or run
// again from disk starts clean. `clear_all` is shutdown.
void clear(int owner) noexcept;
void clear_all() noexcept;

// One script's records, newest-first is not promised: in the order they were first seen. Valid until
// the next report() or clear().
[[nodiscard]] std::span<const entry> entries(int owner) noexcept;

// Whether the script said tw.pending recently enough that it is still waiting for something - a
// pending record is a statement about now, so one that stopped being repeated stops counting.
[[nodiscard]] bool waiting(int owner) noexcept;

// Short display name of a level, for the tab.
[[nodiscard]] const char* name(level severity) noexcept;
} // namespace tw::lua::diag
