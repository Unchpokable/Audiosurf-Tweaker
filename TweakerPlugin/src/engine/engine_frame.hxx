#pragma once

// The engine's own frame: a counter that advances once per `EngineControl::EngineLoop`, and the
// clock that goes with it.
//
// Why this exists next to the overlay's frame counter rather than instead of it: they are different
// frames. The overlay's advances in Present, which does not happen while the game is minimised, does
// not happen before there is a device, and happens at whatever rate the machine reaches - 180 Hz is
// a real number on a real player's machine (lua-scripting.md §9.1). The engine's advances exactly
// when the channel graph is evaluated, which is what a script means when it says "per frame".
//
// Everything here is written from the engine thread only (engine_control's detour) and read from
// anywhere, so the counter is a relaxed atomic and nothing else needs synchronising: the values are
// independent, and a reader that catches an old count is a reader that is one frame behind, which is
// what it would be anyway.
namespace tw::engine::frame
{
// Engine thread, from engine_control, at the top of each frame and before any subscriber runs.
void begin() noexcept;

// Frames since the spine went in. Starts at 0 and becomes 1 on the first EngineLoop, so a non-zero
// value and started() say the same thing.
[[nodiscard]] std::uint32_t count() noexcept;

// Whether the graph has been evaluated at least once with the spine installed. This is the answer to
// "did the hook actually go where we think it did", and load.cxx reports it in the lifecycle log.
[[nodiscard]] bool started() noexcept;

// Seconds between the last two engine frames, clamped to something an animation can use. Zero until
// two frames have gone by.
//
// Deliberately not the overlay's dt: during a load the graph can tick while nothing is presented,
// and the two clocks disagree by however long that lasted.
[[nodiscard]] float dt_seconds() noexcept;

// The engine's own graph counter, `EngineInterface::GetTreeCalculateCount()`, or -1 when the engine
// pointer has not been captured yet.
//
// **Rings at 30000** (boot journal §1.2) - compare for equality, never for order. It is not the same
// number as count(): this one is the engine's, ours starts when we attach, and a group's own counter
// is added on top of it per group (`A3d_ChannelGroup::GetTreeCalculateCount`). What it is for is
// reading a channel's `channelCalculatedAtCount_` against it, which is Ф3.
[[nodiscard]] int tree_count() noexcept;
} // namespace tw::engine::frame
