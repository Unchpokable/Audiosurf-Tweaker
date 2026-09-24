#pragma once

// The one answer to "can I do this yet".
//
// **This replaces a heuristic that was right for the wrong reason.** The old write gate in
// lua_api.cxx waited for the engine's group count to stop moving for a second, and it worked - but
// it was an observation of loading's side effect, with no idea what it was observing, and it could
// say nothing at all about the states after the game came up. What it could not do, and what Ф2 of
// Docs/Internal/lua-engine-fix-roadmap.md exists to fix, is hold a script's callbacks back: scripts
// ran from the first frame the plugin had, which is the middle of the game assembling itself.
//
// **The signal is the game's own decision, not a value read out of a channel.** Audiosurf's loader
// hands control over by calling `SetNewStartChannel`, which points the engine's start group at
// `XX_StartHere.cgr`, and it does that exactly when its own progress channel reaches 1
// (reversing-journal-boot.md §2.3, §2.4). Reading a state channel instead does not work and cannot
// be made to: a `.cgr` stores the value the *editor* last saved, so `StartupState` reads "main menu"
// from the instant the file is mapped, which is the middle of the window this is meant to protect
// (§3 of the same journal).
//
// That signal is a **state**, not an event, which is what makes late injection work: a plugin that
// arrives in an already-running game reads the same two calls and is `ready` on its first frame.
//
// **The trap this protects, stated once and kept:** writing into the graph while the game is still
// loading does not crash it, it makes it quietly wrong. Observed, from a script whose only stated
// job was recolouring tiles: a broken track generator, and characters swapped around in the menu.
// Nothing connects the symptom to the cause, and the player has no way to guess.
namespace tw::engine::state
{
enum class phase : int {
    // No EngineLoop has arrived. Either the spine is not in, or the game has not started its pump.
    detached = 0,

    // The graph is running, and the group it starts from is a loader - the game is still coming up.
    booting = 1,

    // The start group is the project root, but the set of loaded groups is still moving. Channels
    // may be resolved; nothing may be run and nothing may be written.
    starting = 2,

    // Everything is allowed.
    ready = 3,

    // Was ready, and groups are moving again - a run loading, the `Renderer` pool being rebuilt.
    // Callbacks keep running; this is a normal event that happens on every single run, not a fault.
    busy = 4,
};

// Wires the state machine to the frame spine and the group registry. Call once, from startup, after
// engine::control::install(). False only when the spine itself is not there.
bool initialize() noexcept;

// Subscribed to the spine's pre-graph phase, immediately after groups::tick().
void tick() noexcept;

[[nodiscard]] phase current() noexcept;

// "detached" / "booting" / "starting" / "ready" / "busy". Module-owned, never null - this is what
// tw.state() hands to scripts and what the Scripts tab prints.
[[nodiscard]] const char* name(phase value) noexcept;
[[nodiscard]] const char* current_name() noexcept;

// `ready` or `busy`: the graph is up and a script may run and write. The single predicate everything
// used to ask three different questions to approximate.
[[nodiscard]] bool ready() noexcept;

// Bumped on every transition. Cheap "has it moved since I looked", for the UI and for the dispatcher
// that has to fire on_ready exactly once.
[[nodiscard]] std::uint32_t revision() noexcept;

// How many channel groups are loaded right now - the number the Scripts tab shows while waiting, and
// the only honest progress indication there is until the loader's own progress channel is confirmed
// readable (lua-engine-fix-roadmap.md §4.3).
[[nodiscard]] int group_count() noexcept;

// The start group's file name, for diagnostics: this is the signal itself, so showing it is how a
// user with a broken session tells "waiting for the loader" from "waiting for nothing".
[[nodiscard]] const char* start_group() noexcept;

// The settle window: the roster must have stood still for this many engine frames **and** this many
// milliseconds. Both, because neither is trustworthy alone - the engine ran at 625 Hz during one
// measured load and at the screen's refresh rate in the menu, so 30 frames is anywhere between 48 ms
// and half a second (lua-engine-fix-roadmap.md §4.1).
void configure(int frames, int milliseconds) noexcept;
[[nodiscard]] int settle_frames() noexcept;
[[nodiscard]] int settle_ms() noexcept;
} // namespace tw::engine::state
