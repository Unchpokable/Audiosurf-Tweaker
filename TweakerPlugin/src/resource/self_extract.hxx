#pragma once

#include "resource/resource_type.hxx"

// HMODULE / STL types come from pch.hxx (resource.cxx) — do not include
// windows.h / STL here.

namespace tw::resource::self_extract
{
struct entry
{
    type kind {};
    std::string key;
    std::span<const std::byte> bytes;
};

// Enumerate PE resources (TW_TEXTURE / TW_FONT / TW_TEXT) into `out`. Spans
// point at LockResource memory owned by the module — valid until unload.
bool run(HMODULE module, std::vector<entry>& out) noexcept;

void clear(std::vector<entry>& out) noexcept;
} // namespace tw::resource::self_extract
