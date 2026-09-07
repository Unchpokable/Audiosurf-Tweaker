#pragma once

// Where the engine writes, in a sky shader's constant file.
//
// Its own header because this is a contract with two ends and neither owns it: one end is
// assets/shaders/sky_common.hlsli, which declares the registers, and the other is the draw path,
// which fills them. Keeping the numbers inside sky_program.hxx put them behind that header's whole
// dependency tree - the compiler, the package, the parameters - for anything that only wanted to
// know which registers are the engine's.
//
// Every number here is part of the shader ABI. Changing one silently breaks every sky already
// written against it, which is why they are constants with names rather than literals at the call
// sites.
namespace tw::skybox
{
// The clock. x = seconds since the shader path started, y = program-specific.
inline constexpr int k_runtime_register = 5;

// The music: g_music, g_music_time, g_music_eq, g_music_slow. See plugin/music.hxx for what is in
// them, and Docs/Internal/reversing-journal-gameplay.md §10 for where the numbers come from and what
// the game's FFT can and cannot tell apart.
inline constexpr int k_music_register = 208;
inline constexpr int k_music_registers = 4;

// c208..c223 belongs to the engine. Nothing an author writes may live here.
//
// High, and reserved in bulk, because the low registers are handed out by hand - `Our Drafts
// Collides v2` declares c6..c45 with gaps in it - so a low number for an engine-written block would
// collide with somebody's knob the first time a sky grew one. Three of the sixteen are spoken for;
// the rest exist so the next block does not have to go looking for another hole.
//
// The reservation is checked rather than assumed: harness/music asserts that an ordinary sky
// declares nothing in this range. It matters because fxc packs a shader's own `def` literals into
// the same register file, so a collision would corrupt the program rather than misconfigure it.
inline constexpr int k_reserved_engine_register = 208;
} // namespace tw::skybox
