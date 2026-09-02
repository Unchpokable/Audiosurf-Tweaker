#pragma once

#include "skybox/sky_package.hxx"

// What the user changed about one sky, in one file per sky.
//
//   <plugin dir>/Skies/OurDraftsCollides.json
//
// Replaces the `param.*` keys in TweakerPlugin.skybox.cfg, and not only because those were
// unreadable. That file **accumulates settings for skies that no longer exist**, permanently: it
// keeps unrecognised `param.*` keys and writes them back untouched, which it has to, because from
// inside there is no way to tell "a sky that is not loaded right now" from "a sky that was deleted".
// One file per sky answers that by construction - delete the sky, delete its settings - and makes a
// tuning something you can send somebody.
//
// Kept beside the plugin rather than inside the package. A package may be a zip and therefore not
// writable at all, and more importantly it belongs to the sky's *author* while these values belong
// to whoever is playing. Writing a player's numbers into somebody else's shipped content is wrong
// even where it happens to be possible.
//
// An entry is identified by its layer and the parameter's `id` - which defaults to the variable
// name and exists precisely so that renaming the variable does not orphan the value. Deliberately
// *not* by group: a group is a presentation fact, and keying on it would mean re-heading a
// parameter in Config.json silently loses its setting. Groups are still written, as comments, for
// whoever opens the file.
namespace tw::skybox::settings
{
struct entry {
    std::string layer;
    std::string id;

    int count { 1 };
    std::array<float, 3> value {};
};

class store {
public:
    // Missing or unreadable file is not an error - it means nothing has been changed yet.
    void load(const std::filesystem::path& path);

    // Writes every entry, ordered and commented to match `sky`'s own layout, followed by anything
    // held for a layer or parameter the manifest no longer mentions. Those are kept rather than
    // dropped: a layer switched off for an evening must not lose its tuning.
    void save(const package::manifest& sky) const;

    // The same, for a sky that has no manifest to be ordered by: a lone .hlsl, or one of the three
    // programs built into the plugin. Entries come out in the order they were read or first set,
    // and without group headings, since nothing here knows what the groups are.
    //
    // It exists so that *every* sky keeps its settings the same way. The alternative - a flat
    // `param.*` section in the shared config for these and a file per sky for the rest - is what was
    // here before, and it accumulated settings for skies that no longer existed forever, because
    // from inside it there is no way to tell "not loaded right now" from "gone".
    void save(std::string_view display_name) const;

    [[nodiscard]] bool lookup(std::string_view layer, std::string_view id, std::array<float, 3>& out) const noexcept;
    void assign(std::string_view layer, std::string_view id, const std::array<float, 3>& value, int count);

    [[nodiscard]] bool empty() const noexcept
    {
        return m_entries.empty();
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept
    {
        return m_path;
    }

private:
    std::filesystem::path m_path;
    std::vector<entry> m_entries;
};

// <plugin dir>/Skies/<stem>.json.
//
// Named after the package rather than after the `name` inside its manifest. The package's own file
// name is unique in its folder by construction and is under the *user's* control; a manifest name
// can collide between two skies and can change when the author updates one, and either way the
// settings would orphan with nothing said.
[[nodiscard]] std::filesystem::path path_for(std::string_view package_stem);
} // namespace tw::skybox::settings
