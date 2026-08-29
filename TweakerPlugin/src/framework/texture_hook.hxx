#pragma once

// Interception of Quest3D's texture channel, i.e. the point where a file on disk becomes an
// IDirect3DTexture9 the game will later bind.
//
// Audiosurf does not hand D3DX a path. The sky textures are loaded from Lua:
//
//     q.LoadTextureDQ("textures\\Skysphere_White.png", Tex_WhiteSkysphere, "")
//
// which queues the request on Aco_DynamicLuaLoading; that reads the file itself and pushes the
// finished buffer into the target channel through Aco_DX8_Texture::LoadTextureFromMemoryAndCopyBuffer
// (vtable +0x5C), which in turn calls LoadTextureFromMemory (+0x58), which is where
// D3DXCreateTextureFromFileInMemoryEx finally runs. So there is no file name at the point the
// texture object is born - but there IS a channel name, and it is stable across skins and across
// the game's own White/Grey/Black variants ("Tex_WhiteSkysphere", "Tex_GreySkysphere",
// "Tex_BlackSkysphere", all defined in Render/CopyPasteBuffer.cgr).
//
// LoadTextureFromMemory is therefore the one funnel worth hooking: every path into the channel -
// LoadTextureFromFile, LoadTextureFromMemoryAndCopyBuffer, RestoreTexture - goes through it, it is
// exported by name from the Texture channel DLL (so no signature scanning), and it fires once per
// texture per song start, which makes it firmly cold-path.
namespace tw::framework::texture
{
// `texture` is the channel's freshly created IDirect3DTexture9 - non-owning, and only valid until
// the same channel loads again or the device goes away. `channel_name` may be null if the engine
// has none for this channel.
//
// `about_to_load` fires *before* the original runs, i.e. while the channel still holds the texture
// it is about to release. Subscribers caching a texture pointer per channel must use it to drop
// that pointer: the gap between the release inside LoadTextureFromMemory and the `loaded` callback
// spans real frames (a reload happens during a loading screen, which still renders), and a stale
// pointer left in a cache across that gap can alias a freshly allocated, entirely unrelated texture.
using texture_about_to_load_fn = void (*)(Aco_DX8_Texture* channel, const char* channel_name);
using texture_loaded_fn = void (*)(Aco_DX8_Texture* channel, const char* channel_name, IDirect3DTexture9* texture);

// Registration is additive and permanent, like wndproc_hub's. Must happen before
// install_texture_hook() publishes the hook to the game's threads.
void subscribe(texture_about_to_load_fn about_to_load, texture_loaded_fn loaded) noexcept;

// Resolves the exported entry points and detours LoadTextureFromMemory. Fails (and logs) when the
// Texture channel DLL is not mapped yet - the game loads channel DLLs on demand, so a plugin
// injected before the first group that uses textures can legitimately be too early. Safe to call
// again in that case; a successful install is idempotent and reports true without re-detouring.
bool install_texture_hook() noexcept;

[[nodiscard]] bool is_installed() noexcept;

// Thin wrappers over the exported, non-virtual A3d_Channel::GetChannelName and the virtual
// Aco_DX8_Texture::GetTexture. Resolved by GetProcAddress rather than linked, so the plugin keeps
// no import on HighPoly.dll or the channel DLL - see install_texture_hook(). Both return null
// before a successful install.
[[nodiscard]] const char* channel_name(A3d_Channel* channel) noexcept;
[[nodiscard]] IDirect3DTexture9* channel_texture(Aco_DX8_Texture* channel) noexcept;
} // namespace tw::framework::texture
