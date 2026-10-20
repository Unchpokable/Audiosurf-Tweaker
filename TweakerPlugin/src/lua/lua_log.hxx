#pragma once

// The script log, engine\TweakerStuff\Logs\scripts-<started>.log: what the scripting layer said this session, as a
// file someone can read after the fact - an author, or an AI agent that cannot see the game
// (Docs/Internal/dev-bundle-roadmap.md §3.4).
//
// The notefeed and the Scripts tab are for whoever is looking at the game right now, and both forget: the feed
// scrolls, the tab keeps 24 records per script and only the latest wording of each. This keeps the sequence -
// "saved -> rebuilt -> reloaded -> failed on frame 4711 at hud.lua:88" - with a wall-clock time and the engine
// frame on every line.
//
// What goes in (the sources call write(); nothing here decides what is worth saying):
//  - lua_diag: the first occurrence of every record, whatever its level, in full - traceback included, positions
//    already mapped back to a project's modules; every suspension; a repeated record again at its 10th, 100th,
//    1000th... occurrence; every tw.notify that reached the notefeed;
//  - lua_registry: scripts loaded, failed, switched on and off, reloaded, found and lost by a rescan, the run
//    order;
//  - lua_sched: Resume, and every change of the engine's state (busy <-> ready is every run loading).
//
// **Nothing here makes a system call on the calling thread** (the render-thread rule, CLAUDE.md): write() formats
// into a stack buffer and copies into a ring under a spin lock it shares only with the drain, which holds it for
// one memcpy. The file is written by a thread of this module's own, ten times a second. A line that does not fit
// in the ring is dropped and counted, and the count is written in its place - the log may lose lines under a
// flood, it never stalls a frame.
//
// flush() is the exception, for cold paths only: a reload or a toggle has just read a file from disk, and
// whatever it caused should reach the log before the game has a chance to crash on it.
//
// One session, one file, named by when it started: `scripts-2026-10-18_20-41-43.log`. A file is never reused or
// renamed, so the one from a session that went wrong can be attached to an issue as it is, after any number of
// restarts - the owner's call (dev-bundle-roadmap.md, «Ф3 — итоги»), over the scripts.log + scripts.1.log pair this
// started with. The names sort in time order, so "the latest" is the last one by name. The newest k_keep_sessions
// are kept; open() deletes the older ones, and only files whose name has exactly that shape - a renamed or copied
// log is the user's. A session that writes more than k_session_limit stops at a line saying so.
//
// write() may be called from the startup thread (the first load) and the engine/render thread; the two never
// overlap in time, and the lock would cover it if they did.
namespace tw::lua::log
{
// Starts this session's file in `logs_dir` with `header` as its first line, and the writer thread. Cold, called
// once from lua_host. False if the file cannot be created - write() is then a no-op for the session.
bool open(const std::filesystem::path& logs_dir, std::string_view header) noexcept;

// Stops the writer and writes what is still queued. write() is a no-op afterwards.
void close() noexcept;

// One entry. `owner` is a script id, or -1 for the layer itself; the script's file name is looked up here.
// `kind` is one short word - error, warning, info, waiting, suspended, repeated, notify, script, engine - and
// `where` the place it was said (a callback, an API, `file:line`), empty for none. `text` may span lines: the
// ones after the first are indented under it.
void write(int owner, std::string_view kind, std::string_view where, std::string_view text) noexcept;

// A script's own event, kind `script`: "loaded", "reloaded: failed, not running".
void event(int owner, std::string_view text) noexcept;

// write() with a message formatted into a stack buffer, for the per-frame callers that have numbers to say.
template<typename... TArgs>
void writef(int owner, std::string_view kind, std::string_view where, std::format_string<TArgs...> fmt, TArgs&&... args) noexcept
{
    std::array<char, 512> buffer;
    const auto result = std::format_to_n(buffer.data(), buffer.size(), fmt, std::forward<TArgs>(args)...);
    const auto length = std::min(static_cast<std::size_t>(result.size), buffer.size());
    write(owner, kind, where, std::string_view { buffer.data(), length });
}

// Writes everything queued, on the calling thread. Cold paths only - it takes a lock the writer thread holds
// while it is in WriteFile.
void flush() noexcept;

// Whether a session file is open.
[[nodiscard]] bool is_open() noexcept;

// This session's file; empty before open() and after close(). Cold, the thread that opens and closes - for the
// tab and for tests, not per frame.
[[nodiscard]] const std::filesystem::path& path() noexcept;
} // namespace tw::lua::log
