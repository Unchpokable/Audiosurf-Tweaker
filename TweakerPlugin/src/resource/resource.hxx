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
} // namespace tw::resource
