#pragma once

// The Lua half of the scripting layer: the code that runs before any user script and builds `tw`.
//
// Kept as text in its own file, split into sections, rather than as one literal inside the VM code:
//
//  - **it is most of the layer.** Handles, the drawing API, the registration of every callback and
//    the dispatchers that call them are Lua; the VM around them is a few hundred lines of C. One file
//    for each makes both readable;
//  - **sections are separate chunks.** A runtime error names the section ("tw.channels:212:") instead
//    of an anonymous string, and no single literal grows towards the compiler's limit;
//  - **harness/lua/ljtest reads the text straight out of lua_prelude.cxx** - every R"LUA( block, in
//    order - and runs exactly what the plugin runs. That mechanism is older than this file and is the
//    reason the file exists in this shape: a copy of the prelude in the harness would drift.
//
// How the sections talk to each other and to C (lua_vm.cxx does the driving):
//
//  - each section is called with one argument, the prelude's private table `S`. Nothing in it is
//    visible to a script;
//  - `S.ptrs` arrives holding every C entry point **by name** (lua_vm.cxx has the one list). The
//    first section binds them all against their signatures, refuses to start if either side has a
//    name the other lacks, and leaves the casts in `S.C` for the others;
//  - the last section leaves in `S.exports` the functions C calls back - the dispatchers, script
//    loading and unloading. C takes references to those and nothing else survives.
namespace tw::lua::prelude
{
struct section {
    const char* chunk_name; // "=tw.core" - the '=' makes Lua use it verbatim in error positions
    std::string_view source;
};

// In the order they must run.
[[nodiscard]] std::span<const section> sections() noexcept;
} // namespace tw::lua::prelude
