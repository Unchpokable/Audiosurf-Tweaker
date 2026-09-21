#pragma once

// Module load/unload notifications from the Windows loader (ntdll's LdrRegisterDllNotification).
//
// Exists for the early load from engine\channels\: the plugin is mapped before d3d9.dll and the Texture
// channel, and has to hook both the moment they map - before whoever asked for them gets to run a line of
// their code. Polling loses that race (Docs/Internal/plugin-offline-mode.md, Ф0).
//
// Every handler runs UNDER THE LOADER LOCK, on whichever thread is loading or unloading the module - not
// necessarily the game's main thread. A handler must not wait on another thread in any way: no window
// functions that send messages, no waits, no COM, no suspending threads. What it may do is listed in
// plugin-offline-mode.md §4.2, "Правила для кода под loader lock"; this module adds nothing on top -
// it neither logs nor waits.
namespace tw::framework::loader_watch
{
using module_fn = void (*)(HMODULE module);

struct watch {
    // Base name, compared case-insensitively ("d3d9.dll").
    std::wstring_view base_name;
    module_fn on_loaded;
    module_fn on_unloaded;
};

// Once per process, from DllMain, before any watched module can map. `watches` is read from the loader
// callback for the rest of the process and must outlive it - a static array. The module that registers
// must be pinned first: a callback left behind in an unmapped image is called on the next module load.
bool start(std::span<const watch> watches) noexcept;

// Keeps `module` mapped until the process exits. The plugin hooks code inside d3d9.dll and the Texture
// channel and has no unhook path, so it takes the same kind of reference on them that it holds on itself -
// see plugin-offline-mode.md, Р-25. Loader-lock safe: it only adds a reference to an image already loaded.
bool pin(HMODULE module) noexcept;
} // namespace tw::framework::loader_watch
