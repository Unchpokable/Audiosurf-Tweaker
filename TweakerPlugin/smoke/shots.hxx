#pragma once

#include <string>
#include <vector>

// The snapshot stand: smoke_test's non-interactive half. Renders scenes (scene.hxx, scenes.cxx) into an
// offscreen framebuffer and writes PNGs, and compares two directories of them pixel by pixel. Exists so a
// change to the overlay UI can be checked without the game: "nothing moved" is two identical directories,
// "this moved and nothing else did" is a diff confined to the scenes it should touch.
// Docs/Internal/tweaker-plugin-widgets.md, "Снимочный стенд".
//
//   smoke_test --list                         scene names and what each shows
//   smoke_test --shots <dir> [--filter <s>]   every scene (or those whose name contains <s>), each in a
//                                             process of its own, into <dir>
//   smoke_test --scene <name> --out <dir>     one scene in this process (what --shots spawns)
//   smoke_test --compare <a> <b> [--diff <d>] compare two shot directories; with --diff, write a
//                                             difference image for every file that differs
//
// Exit codes: 0 success / identical, 1 a scene failed / directories differ, 2 bad arguments or an
// environment problem (no GL context, unreadable file).
namespace tw::smoke::shots
{
int run(const std::vector<std::wstring>& args);
} // namespace tw::smoke::shots
