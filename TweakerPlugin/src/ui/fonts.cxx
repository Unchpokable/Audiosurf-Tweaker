#include "ui/fonts.hxx"

// No TweakerPlugin PCH here: this TU is shared with smoke_test, which does not use it, and
// tweaker_ui is not compiled with the DirectX include paths the PCH needs. Same convention as
// ui/texture_cache.cxx - windows.h explicitly, because resource.hxx declares HMODULE-typed
// functions without including it itself.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// resource.hxx relies on its consumers having these in scope (see texture_cache.cxx).
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "resource/resource.hxx"

#include <iterator>

namespace
{
struct face {
    const char* name;
    const char* key;
    ImFont* font;
};

// Appended to, never reordered: an index is part of the script-facing ABI (see fonts.hxx).
//
// Only the weights something actually asks for are here. Every Roboto weight is packed into the DLL
// already (src/resource/CMakeLists.txt globs assets/fonts/*.ttf), so adding one is a line here - but
// each baked face costs atlas space whether or not anything draws with it, which is why this is not
// simply "all of them".
face g_faces[] = {
    { "regular", "fonts/Roboto-Regular.ttf", nullptr },
    { "semibold", "fonts/Roboto-SemiBold.ttf", nullptr },
};

ImFont* bake(ImFontAtlas* atlas, const char* key, float size_px) noexcept
{
    const auto packed = tw::resource::get_resource(tw::resource::type::font, key);
    if(!packed || packed->bytes.empty()) {
        return nullptr;
    }

    ImFontConfig config;
    // The bytes live in the PE image's own resource memory, which is never freed and which ImGui
    // must not try to free either.
    config.FontDataOwnedByAtlas = false;

    return atlas->AddFontFromMemoryTTF(
        const_cast<void*>(static_cast<const void*>(packed->bytes.data())), static_cast<int>(packed->bytes.size()), size_px, &config);
}
} // namespace

namespace tw::ui::fonts
{
bool load(ImFontAtlas* atlas, float size_px) noexcept
{
    if(atlas == nullptr) {
        return false;
    }

    for(face& entry : g_faces) {
        entry.font = bake(atlas, entry.key, size_px);
    }

    return g_faces[0].font != nullptr;
}

int count() noexcept
{
    return static_cast<int>(std::size(g_faces));
}

const char* name(int index) noexcept
{
    if(index < 0 || index >= count()) {
        return nullptr;
    }

    return g_faces[index].name;
}

ImFont* at(int index) noexcept
{
    if(index > 0 && index < count() && g_faces[index].font != nullptr) {
        return g_faces[index].font;
    }

    return g_faces[0].font;
}
} // namespace tw::ui::fonts
