#pragma once

// The Lua half of the scripting layer: the code that runs before any user script and builds `tw`.
//
// The sections are Lua files, src/lua/prelude/<section>.lua, compiled into the DLL as text by
// cmake/EmbedText.cmake; lua_prelude.cxx only lists them, in the order they run:
//
//  - **it is most of the layer.** Handles, the drawing API, the registration of every callback and
//    the dispatchers that call them are Lua; the VM around them is a few hundred lines of C. Each
//    in files of its own language makes both readable, and the Lua gets an editor that knows Lua;
//  - **sections are separate chunks.** A runtime error names the section ("tw.channels:212:") instead
//    of an anonymous string;
//  - **harness/lua/ljtest runs the same files**, in the order k_sections in lua_prelude.cxx gives - it
//    reads that table - so it runs exactly what the plugin runs. A copy of the prelude in the harness
//    would drift.
//
// Until 20.10.2026 the sections were raw string literals inside lua_prelude.cxx itself
// (Docs/Internal/script-tabs.md §3.9).
//
// How the sections talk to each other and to C (lua_vm.cxx does the driving):
//
//  - each section is called with one argument, the prelude's private table `S`. Nothing in it is
//    visible to a script;
//  - `S.ptrs` arrives holding every C entry point **by name** (lua_vm.cxx has the one list). The
//    first section binds them all against their signatures, refuses to start if either side has a
//    name the other lacks, and leaves the casts in `S.C` for the others;
//  - tw.callbacks leaves in `S.exports` the functions C calls back - the dispatchers, script loading
//    and unloading. C takes references to those and nothing else survives;
//  - tw.sandbox, the last section, takes away what a script must not reach. Everything above it has
//    already captured what it needs from those names.
namespace tw::lua::prelude
{
struct section {
    const char* chunk_name; // "=tw.core" - the '=' makes Lua use it verbatim in error positions
    std::string_view source;
};

// In the order they must run.
[[nodiscard]] std::span<const section> sections() noexcept;
} // namespace tw::lua::prelude
