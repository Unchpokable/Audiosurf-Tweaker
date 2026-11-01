#pragma once

namespace tw::skybox::package
{
struct manifest;
} // namespace tw::skybox::package

// One package selection and its disk revision. Reading/stat-ing happens in bg_work; the frame
// only collects a ready result. Both the menu watch and a repeated SKY_SELECT use request(), so
// an archive already read (even one still compiling, or one that failed) is not loaded twice.
namespace tw::skybox::reload
{
struct update {
    std::shared_ptr<const package::manifest> sky;

    bool missing {};
};

void select(std::filesystem::path path);

void clear() noexcept;

void request() noexcept;

[[nodiscard]] std::optional<update> poll(bool watch);

[[nodiscard]] bool loading() noexcept;
} // namespace tw::skybox::reload
