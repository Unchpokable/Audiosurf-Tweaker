#pragma once

// Compiling HLSL to ps_3_0 inside the game process, so a sky can be edited without rebuilding the
// plugin.
//
// This is deliberately *not* how the bundled programs get their bytecode - those are compiled by
// fxc at build time and embedded, so they work on a machine with no compiler at all. This path
// exists for the user's own .hlsl, where the alternative is no path.
//
// The compiler is d3dcompiler_47.dll, loaded on demand: it ships with Windows 10 and 11 and was
// confirmed present with D3DCompile exported (see the Phase 0 results in
// Docs/Internal/skybox-procedural.md). d3dcompiler_43.dll, which the DirectX SDK redistributed, is
// accepted as a fallback. When neither is there, this module simply reports itself unavailable and
// the built-in programs carry on working.
namespace tw::skybox::compile
{
// Loads the compiler if it is not loaded yet. Cold path, and the only place that can block for a
// noticeable time (a LoadLibrary).
[[nodiscard]] bool available() noexcept;

// Which DLL answered, for the log and the overlay. Empty until available() has been called.
[[nodiscard]] std::string_view backend_name() noexcept;

struct result {
    std::vector<std::byte> bytecode;

    // The text that was compiled, carried back because the worker has already read it and the main
    // thread needs it too: the shader's parameter annotations live in its source (see sky_params),
    // and they can only be resolved once the bytecode exists to name registers with.
    std::string source;

    // Whatever the compiler said. Non-empty on failure; can also be non-empty on success, when the
    // shader compiled with warnings.
    std::string diagnostics;

    [[nodiscard]] bool ok() const noexcept
    {
        return !bytecode.empty();
    }
};

// Compiles `source` as a ps_3_0 shader with entry point `main`. `source_path` is used for the
// compiler's own diagnostics and as the base directory for #include - and #include also reaches the
// headers packed into the DLL, so a user's shader can say `#include "sky_common.hlsli"` and get the
// same constant layout the bundled programs use.
[[nodiscard]] result pixel_shader(std::string_view source, const std::filesystem::path& source_path);

// Reads a file and compiles it. Separate from the above only because "could not open the file" is a
// different kind of failure than "did not compile", and both have to reach the user the same way.
[[nodiscard]] result pixel_shader_file(const std::filesystem::path& path);

// The same, on a worker thread.
//
// This is the form callers should use, because the render thread is where they all live. A shader
// worth writing takes seconds to compile - the DyingLight port measured five to seven - and doing
// that inside EndScene freezes the game for exactly as long. Nothing here touches the device, so
// there is nothing to serialise against: the compiler reads a file, reads the packed headers (an
// immutable index by the time any of this runs) and returns bytes.
[[nodiscard]] std::future<result> pixel_shader_file_async(std::filesystem::path path);
} // namespace tw::skybox::compile
