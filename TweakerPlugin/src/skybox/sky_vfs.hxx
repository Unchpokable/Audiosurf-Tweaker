#pragma once

// Reading files out of a `.sky` package, whatever form the package takes.
//
// Two reasons this exists rather than each caller opening an ifstream on `root / name`.
//
// **The archive form is coming.** A package is a directory during development and a zip when it is
// distributed (Docs/Internal/sky-package.md), and every consumer - the manifest loader, the shader
// compiler, the image loader, a generator script - should read bytes without knowing which. Note that
// the decoders already cooperate: stb is used through its `*_from_memory` entry points throughout,
// so nothing downstream ever sees a path.
//
// **Names will come from scripts.** A generator script asks for its own assets by name, and a name
// from a script is untrusted input. The confinement below is therefore not a nicety - it is the
// boundary, and it is far easier to get right once, here, than at each call site.
//
// # What confinement means for a directory
//
// For the archive form the question does not arise: there is no way to name something outside a zip.
// For a directory it does, and the traps are more numerous than "reject ..":
//
//  - absolute paths, and drive-relative ones - `C:foo` is neither absolute nor relative to the root;
//  - UNC paths, `\\server\share`;
//  - reserved DOS device names - `NUL`, `CON`, `COM1` - which open *successfully* at any depth;
//  - alternate data streams, `clouds.png:payload`;
//  - symlinks and junctions inside the package pointing out of it. This is the one that cannot be
//    caught by inspecting the string, and the reason the check resolves the path before comparing;
//  - and comparing by string prefix, which lets `/skies/foo-evil` pass a test for `/skies/foo`.
//
// So the rule is: resolve to a canonical absolute path, then compare **component by component**
// against the canonical root.
namespace tw::skybox::package
{
// The zip an archive package was opened from: its bytes, its entry index, and the miniz reader over
// them. Opaque here on purpose - miniz.h stays out of every header that includes this one.
struct archive;

class file_system
{
public:
    file_system() = default;

    // A package that is an unpacked directory. The root is canonicalised once, here, so that every
    // later comparison is against a path with no symlinks and no `..` left in it.
    [[nodiscard]] static file_system open_directory(const std::filesystem::path& root);

    // A package that is a zip. The whole archive is read into memory here, which is what makes every
    // read below thread-safe against the file being replaced - and a package is replaced by being
    // rewritten, so a reader holding the file open would be a reader stopping its own author from
    // saving.
    //
    // A zip whose entries all sit under one top-level folder is read as though they did not: zipping
    // the package folder and zipping its contents are both what people do, and the difference is not
    // one an author should have to know about.
    [[nodiscard]] static file_system open_archive(const std::filesystem::path& file);

    [[nodiscard]] bool is_open() const noexcept
    {
        return !m_root.empty();
    }

    [[nodiscard]] bool is_archive() const noexcept
    {
        return m_archive != nullptr;
    }

    // What the package *is* on disk: the directory, or the archive file itself. For naming it, for
    // stamping its settings and for the reload watch.
    //
    // Never for building a path to something inside it - that is locate()'s job, and it is the one
    // that knows an archive has no such paths.
    [[nodiscard]] const std::filesystem::path& root() const noexcept
    {
        return m_root;
    }

    // Reads one file, named relative to the package root with `/` separators as the manifest writes
    // them. False when the name escapes the package, names something that is not a regular file, or
    // cannot be read - the caller cannot tell those apart on purpose, because a script must not be
    // able to probe the filesystem outside its package by watching which error it gets.
    [[nodiscard]] bool read(std::string_view name, std::vector<std::byte>& out) const;

    // The same, as text. Convenience for the manifest and for shader sources.
    [[nodiscard]] bool read_text(std::string_view name, std::string& out) const;

    [[nodiscard]] bool exists(std::string_view name) const;

    // The absolute path of a file inside the package, or empty when the name is not allowed or the
    // form has no paths. Only for things that genuinely need one - the reload watch stats it, and it
    // is what a diagnostic prints when there is something to print.
    //
    // **Always empty for an archive**, and every caller has to mean it: an archived package's files
    // do not exist as paths, and code that treats an empty answer as an error rather than as "read
    // it instead" is code that works for a folder and not for a zip.
    [[nodiscard]] std::filesystem::path locate(std::string_view name) const;

    // When `name` last changed, for the reload watch.
    //
    // For an archive this is the *archive's* own write time, whatever the name: everything inside it
    // changes when it is rewritten, and nothing inside it changes otherwise. Zero when there is no
    // such entry, which is what keeps a manifest naming a file that does not exist from looking like
    // a file that never changes.
    [[nodiscard]] std::filesystem::file_time_type write_time(std::string_view name) const;

private:
    std::filesystem::path m_root;

    // Null for a directory. Shared because a file_system is copied - onto the manifest, into a
    // compile running on a worker thread - and the archive it reads from is neither cheap to
    // duplicate nor sensible to have two of.
    std::shared_ptr<archive> m_archive;
};

// Exposed for testing, and because the rule is worth being able to point at. True when `name` is a
// relative, single-package-local name with no device name, no stream suffix and no `..`.
//
// This is only the cheap half - it rejects what can be rejected by looking at the text. It cannot
// catch a symlink, which is why file_system::locate() resolves and re-checks.
[[nodiscard]] bool is_safe_relative_name(std::string_view name) noexcept;
} // namespace tw::skybox::package
