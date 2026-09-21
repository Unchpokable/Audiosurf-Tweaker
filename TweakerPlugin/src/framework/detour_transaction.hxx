#pragma once

namespace tw::framework::detour
{
struct binding {
    void** target;
    void* replacement;
};

// Which of the process's other threads a transaction suspends while it patches.
//
// `others` is the default and the only safe choice for code some thread may be running right now.
//
// `none` exists for code under the loader lock - DllMain and loader notifications - where suspending a
// thread is how one that holds the process heap lock ends up frozen while Detours allocates. It is only
// correct when nobody can be executing the target: an export of the module being loaded at this very
// moment, or a function nothing has called yet. See Docs/Internal/plugin-offline-mode.md §4.2.
enum class suspend : std::uint8_t {
    others,
    none,
};

bool attach(std::initializer_list<binding> bindings, suspend threads = suspend::others);

// For a set of bindings only known at run time. Still one transaction: all of them or none.
bool attach(std::span<const binding> bindings, suspend threads = suspend::others);
bool detach(std::initializer_list<binding> bindings);
} // namespace tw::framework::detour
