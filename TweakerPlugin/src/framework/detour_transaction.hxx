#pragma once

namespace tw::framework::detour
{
struct binding {
    void** target;
    void* replacement;
};

bool attach(std::initializer_list<binding> bindings);
bool detach(std::initializer_list<binding> bindings);
} // namespace tw::framework::detour
