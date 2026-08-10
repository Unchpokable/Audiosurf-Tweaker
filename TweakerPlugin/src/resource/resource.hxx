#pragma once

#include "resource/resource_type.hxx"

// HMODULE / STL types come from pch.hxx (or an equivalent include set in
// consumers that do not use the plugin PCH, e.g. smoke_test).

namespace tw::resource
{
enum class error {
    not_initialized,
    not_found,
    wrong_type,
};

struct view {
    type kind {};
    std::span<const std::byte> bytes;

    // Interprets `bytes` as a char buffer (no copy). Intended for type::text.
    [[nodiscard]] std::string_view text() const noexcept;
};

bool initialize(HMODULE module) noexcept;
void shutdown() noexcept;

[[nodiscard]] std::expected<view, error> get_resource(std::string_view key) noexcept;
[[nodiscard]] std::expected<view, error> get_resource(type kind, std::string_view key) noexcept;

// Every indexed key of `kind`, in unspecified order. For consumers that bake a whole asset class up
// front instead of looking assets up by name (ui/image/svg). The views point at the index's own key
// storage - valid until the next initialize()/shutdown(). Cold path: this allocates.
[[nodiscard]] std::vector<std::string_view> list_keys(type kind);
} // namespace tw::resource
