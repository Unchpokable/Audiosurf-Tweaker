#pragma once

#include <imgui.h>

// The overlay's baked font faces, addressed by index.
//
// One face per weight - not per size. ImGui 1.92 sizes a face at draw time, so `AddText(font, size,
// ...)` rasterizes at whatever size is asked for; baking a face once covers every size anything
// will ever draw it at. That is the whole reason this is a short list of weights rather than the
// size matrix the pre-1.92 API would have forced.
//
// Lives in ui/ rather than in framework/imgui_backend because smoke_test builds its own ImGui
// context and has to end up with the same faces in the same order - a script laid out against
// index 1 must not mean a different weight in the harness than in the game. The only thing this
// touches is ImFontAtlas, so it stays renderer-agnostic like the rest of ui/.
namespace tw::ui::fonts
{
// Bakes every face into `atlas` at `size_px`.
//
// Index 0 is baked first and becomes ImGui's default face - everything the overlay's own widgets
// draw goes through it. Returns false only when *that* face is missing or unusable, which is fatal
// for the overlay; a secondary face that fails to bake is not, and at() falls back to index 0 for
// it. A script asking for a weight that is not there therefore gets plain text rather than nothing.
bool load(ImFontAtlas* atlas, float size_px) noexcept;

[[nodiscard]] int count() noexcept;

// Stable name for an index, or nullptr when out of range. These are the names scripts pass to
// tw.hud.text - appended to, never reordered.
[[nodiscard]] const char* name(int index) noexcept;

// The face for an index. Falls back to index 0 for an out-of-range index or a face that did not
// bake, so this is null only before a successful load().
[[nodiscard]] ImFont* at(int index) noexcept;
} // namespace tw::ui::fonts
