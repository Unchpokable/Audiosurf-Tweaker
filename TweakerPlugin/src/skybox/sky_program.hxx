#pragma once

// sky_program owns an in-flight std::future<compile::result>, so the result type has to be complete
// here. The only project header this one needs.
#include "skybox/sky_compile.hxx"
#include "skybox/sky_params.hxx"

// The sky programs: a compiled pixel shader plus the constants it is fed.
//
// This is the second kind of sky source, alongside the cube maps sky_cubemap builds. A program
// paints the same cube, but computes its colour per pixel instead of sampling an image - so it is
// sharp at any resolution, costs no memory, and can move. See Docs/Internal/skybox-procedural.md.
//
// Two origins, one type:
//
//  - built-in: compiled by fxc during the build and embedded in the DLL, so they work on a machine
//    with no shader compiler at all.
//  - from a file: a .hlsl under config::skybox_dir(), compiled in-process by sky_compile and
//    recompiled whenever it changes on disk.
//
// Program objects are never moved or destroyed once registered, because the draw path, the shader
// cache and the overlay all hold pointers to them. Reloading a file program rewrites it in place.
namespace tw::skybox
{
// The one register whose contents this frame decides rather than the palette: x = seconds, y =
// program-specific. Fixed by assets/shaders/sky_common.hlsli, and named here so the draw path does
// not have to spell 5 out again.
inline constexpr int k_runtime_register = 5;

struct sky_program {
    std::string id;           // config value and catalog id: a built-in name, or a path
    std::string display_name; // what the overlay shows

    // Compiled ps_3_0. Points into the DLL's resources for a built-in, or into `owned_bytecode` for
    // one compiled from a file.
    std::span<const std::byte> pixel_bytecode;
    std::vector<std::byte> owned_bytecode;

    // The whole ps_3_0 float register file, because that is the only real limit in the chain. c0-c5
    // are the shared palette assets/shaders/sky_common.hlsli describes; c6 and up belong to whatever
    // parameters the shader declares. Owned rather than a view onto a static table, because
    // parameters write into it: a program's constants are its defaults plus whatever the user has
    // moved since.
    //
    // This was sixteen registers, then thirty-two, and each of those numbers had to be repeated
    // wherever the block was sized, masked or uploaded - which is how two of them ended up
    // disagreeing and quietly dropping half a shader. Sizing it to the hardware leaves nothing to
    // choose and nothing to keep in step. 3.5 KB per program buys that outright.
    std::array<float, bytecode::k_float_registers * 4> constants {};

    // What the overlay offers to move. Empty for a program with no annotation block, which is every
    // program until somebody writes one.
    std::vector<sky_param> params;

    // Layout generation from the annotation's `version` line, folded into the settings key so that
    // a rewritten shader does not inherit values keyed to the meanings its registers used to have.
    int param_version {};

    // Bumped every time `params` is rebuilt, which is every compile - including a hot reload that
    // kept the same program object. The overlay caches per-parameter widgets and tab pages keyed on
    // the program pointer, and that pointer alone cannot tell "same shader, recompiled with a
    // different set of knobs" from "nothing happened"; this can. Same device as catalog_generation().
    unsigned int params_generation {};

    // Which registers the shader actually declares, read out of its own constant table rather than
    // promised by its author - see sky_bytecode. Contiguous stretches, ascending, already merged, so
    // the upload is one call each and the gaps between them are never written: those gaps hold the
    // shader's own `def` literals, and writing one corrupts the program rather than configuring it.
    //
    // Genuinely a set with holes in it: a shader is free to read g_runtime at c5 and nothing else,
    // which several do.
    std::vector<bytecode::register_run> constant_runs;

    // True when this program reads its marker intensity out of g_runtime.y (only the probe does).
    bool markers_in_runtime_y {};

    // Empty for a built-in. Set for a file program, and what the reload poll stats.
    std::filesystem::path source_path;
    std::filesystem::file_time_type source_time {};

    // The compiler's last word on this program: empty when it compiled cleanly, otherwise the error
    // (or the warnings). A file program that fails to recompile keeps the bytecode it had, so an
    // edit that does not compile leaves the previous sky up instead of blanking it.
    std::string diagnostics;

    // A compile running on a worker thread. Valid means one is in flight; poll_program collects it.
    std::future<compile::result> pending;

    [[nodiscard]] bool from_file() const noexcept
    {
        return !source_path.empty();
    }

    [[nodiscard]] bool usable() const noexcept
    {
        return !pixel_bytecode.empty();
    }

    [[nodiscard]] bool compiling() const noexcept
    {
        return pending.valid();
    }

    // Whether g_runtime is one of the registers this program reads - the register carrying the
    // per-frame values, as opposed to the palette, which is fixed.
    [[nodiscard]] bool has_runtime() const noexcept
    {
        for(const bytecode::register_run& run : constant_runs) {
            if(k_runtime_register >= run.first && k_runtime_register < run.first + run.count) {
                return true;
            }
        }
        return false;
    }
};

// Builds the built-in programs from the DLL's resources. Must run after tw::resource::initialize.
void initialize_programs();

// Every registered program, built-in first, in the order the overlay lists them. The pointers stay
// valid for the life of the process.
[[nodiscard]] std::span<sky_program* const> programs() noexcept;

// Null when no program carries that id - a typo in the config, or a file that has since been
// deleted.
[[nodiscard]] sky_program* find_program(std::string_view id) noexcept;

// The shared vertex shader, which every program uses: the cube is the cube, and all the variety
// lives in the pixel stage.
[[nodiscard]] std::span<const std::byte> vertex_bytecode() noexcept;

// Compiles the .hlsl at `path` and registers it, or returns the already-registered program for it.
// Registration happens even when the compile fails, so the failure is something the overlay can
// show and a later edit can fix, rather than an entry that vanishes.
sky_program* load_file_program(const std::filesystem::path& path);

enum class program_event {
    none,
    compile_started, // the file changed on disk and a worker thread is on it
    compiled,        // new bytecode installed - the caller must drop any D3D shader made from the old
    compile_failed,  // it did not compile; the previous bytecode is still in place
};

// Per-frame housekeeping for one program: collects a finished background compile, and - only when
// `watch` is set - checks whether the file changed and starts a new one.
//
// The two halves are gated separately on purpose. Collecting has to happen every frame no matter
// what, or a compile started just before the overlay closed would never land. Stat-ing the file is
// worth doing only while somebody is actually iterating on it, which is what `watch` means: a
// filesystem call on the render thread is cheap until an antivirus decides otherwise.
//
// A failure is reported rather than swallowed, and is not the same as "nothing happened": an edit
// that does not compile is exactly the moment its author needs to be told, and the previous sky
// staying up is what makes that survivable rather than alarming.
program_event poll_program(sky_program& program, bool watch);

// Writes `program.params` into `program.constants`. Call after moving a parameter; the next draw
// picks the new values up, with no recompile - they are constants, not code.
void refresh_constants(sky_program& program) noexcept;

// Key a parameter is stored under in the settings file: "param.<program>.<param key>". Built from
// the display name rather than the id, because a file program's id is an absolute path and nobody
// wants that in a settings file.
[[nodiscard]] std::string param_storage_key(const sky_program& program, const sky_param& param);
} // namespace tw::skybox
