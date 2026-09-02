#pragma once

#include "plugin/bg_work.hxx"

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

// Which of the two programmable stages to build. Both are shader model 3.
//
// The vertex stage exists because a geometry layer is a pair: a sky that ships its own clouds ships
// how they are shaped as well as how they are lit, and half of that lives in the vertex shader.
enum class stage {
    pixel,
    vertex,
};

// Compiles `source` with entry point `main`. `source_path` is used for the compiler's own
// diagnostics and as the base directory for #include - and #include also reaches the headers packed
// into the DLL, so a user's shader can say `#include "sky_common.hlsli"` and get the same constant
// layout the bundled programs use.
[[nodiscard]] result shader(std::string_view source, const std::filesystem::path& source_path, stage target = stage::pixel);

// Reads a file and compiles it. Separate from the above only because "could not open the file" is a
// different kind of failure than "did not compile", and both have to reach the user the same way.
[[nodiscard]] result shader_file(const std::filesystem::path& path, stage target = stage::pixel);

// The same, on a worker thread.
//
// This is the form callers should use, because the render thread is where they all live. A shader
// worth writing takes seconds to compile - the DyingLight port measured five to seven - and doing
// that inside EndScene freezes the game for exactly as long. Nothing here touches the device, so
// there is nothing to serialise against: the compiler reads a file, reads the packed headers (an
// immutable index by the time any of this runs) and returns bytes.
//
// Runs on the shared pool (plugin/bg_work) rather than on a thread of its own. The handle it hands
// back can be dropped without blocking, which the std::future this used to return could not:
// abandoning a compile because its program had been replaced would stall the render thread inside
// that future's destructor.
[[nodiscard]] tw::plugin::bg_work::task<result> shader_file_async(std::filesystem::path path, stage target = stage::pixel);
} // namespace tw::skybox::compile
