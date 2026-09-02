#pragma once

// sky_program owns an in-flight bg_work::task<compile::result>, so the result type has to be complete
// here. The only project header this one needs.
#include "skybox/sky_compile.hxx"
#include "skybox/sky_package.hxx"
#include "skybox/sky_params.hxx"
#include "skybox/sky_shared.hxx"

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

    // Compiled vs_3_0, when this program brings its own vertex stage. Empty means "the shared cube
    // vertex shader", which is every `fullsky` sky: they all paint the same cube and all the variety
    // is in the pixel stage.
    //
    // A geometry layer is the case that needs its own. Its vertices are quads with a baked billboard
    // basis rather than cube corners, and a sky that ships its own clouds ships their shape as well
    // as their shading - half of which lives here.
    std::span<const std::byte> vertex_code;
    std::vector<std::byte> owned_vertex_bytecode;

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

    // The vertex stage's own register file, for a layer that compiles its own vertex shader.
    //
    // Sized to vs_3_0 rather than to ps_3_0: the vertex stage offers 256 float constants where the
    // pixel stage offers 224, and sizing each to its own hardware is what keeps this from being a
    // number two files have to agree about - which is how half a shader once went missing.
    std::array<float, bytecode::k_vertex_float_registers * 4> vertex_constants {};

    // Which of those registers the vertex shader declares. Empty for every `fullsky` sky: the shared
    // cube vertex shader takes a matrix at c0 and nothing else, and nothing here configures it.
    std::vector<bytecode::register_run> vertex_constant_runs;

    // Where the vertex shader wants the frame's time, found by name in its own constant table rather
    // than fixed at a register nobody can see. -1 when it declares no such variable, which is a
    // vertex shader that does not animate.
    int vertex_time_register { -1 };
    int vertex_time_component {};

    // What the overlay offers to move. Empty for a program with no annotation block, which is every
    // program until somebody writes one.
    std::vector<sky_param> params;

    // Layout generation from the annotation's `version` line, folded into the settings key so that
    // a rewritten shader does not inherit values keyed to the meanings its registers used to have.
    int param_version {};

    // This layer's `bind` entries, resolved against its own compiled shader. Evaluated against the
    // sky's shared block whenever a shared knob moves and whenever the layer is rebuilt.
    //
    // This is what replaced the sun adapter. A layer no longer publishes where its light is for
    // somebody else to come and fetch; it declares what it wants and the sky answers - which is why
    // a second, third or fourth light costs nothing but a line in the manifest.
    std::vector<resolved_binding> bindings;

    // The sky this program is a layer of, or null for a lone .hlsl.
    //
    // Held by the program rather than looked up from whoever loaded the package, because a recompile
    // has to rebuild the parameter list and would otherwise need to reach back for the manifest at
    // exactly the moment nothing guarantees it is still there. Shared because every layer of one sky
    // reads the same document *and the same shared values*, and none of them owns either more than
    // the others.
    std::shared_ptr<shared::loaded_sky> sky;

    // Which layer of that sky this program is. Empty for a program loaded from a lone .hlsl.
    //
    // The point of the format is that this is the *only* branch: a package layer and a loose shader
    // produce the same sky_program, with the same sky_param list, drawn by the same panel and the
    // same renderer. What differs is where the parameters were read from and where their values are
    // saved, and that is what this answers.
    std::string package_layer;

    [[nodiscard]] bool from_package() const noexcept
    {
        return !package_layer.empty();
    }

    // What this sky's settings file is named after: the package's own directory name, the .hlsl's
    // file name, or - for one of the three built into the plugin - its id.
    //
    // Every sky keeps its settings the same way, and that is the point of there being one function
    // here rather than a branch at each call site. The flat `param.*` section this replaced could
    // not distinguish a sky that was not loaded from one that no longer existed, so it hoarded the
    // settings of deleted skies forever; a file per sky answers that by construction.
    [[nodiscard]] std::string settings_stem() const
    {
        if(sky != nullptr) {
            return sky->stem;
        }

        return source_path.empty() ? id : source_path.stem().string();
    }

    // This program's layer inside its manifest, or null when it is not a package layer at all - or
    // when the manifest was reloaded and no longer has a layer by that id, which a rebuild has to
    // survive rather than dereference.
    [[nodiscard]] const package::layer* package_layer_ref() const noexcept
    {
        if(sky == nullptr || sky->sky == nullptr) {
            return nullptr;
        }

        return sky->sky->find_layer(package_layer);
    }

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

    // The same for the vertex stage, when this program compiles one. Watched separately because it
    // is a separate file and is edited on its own - a cloud whose shape is being reworked should
    // reload without touching its shading.
    std::filesystem::path vertex_source_path;
    std::filesystem::file_time_type vertex_source_time {};

    // The compiler's last word on this program: empty when it compiled cleanly, otherwise the error
    // (or the warnings). A file program that fails to recompile keeps the bytecode it had, so an
    // edit that does not compile leaves the previous sky up instead of blanking it.
    std::string diagnostics;

    // A compile running on a worker thread. Valid means one is in flight; poll_program collects it.
    tw::plugin::bg_work::task<compile::result> pending;
    tw::plugin::bg_work::task<compile::result> pending_vertex;

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
        return pending.valid() || pending_vertex.valid();
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

// Loads one layer of a package as a program.
//
// A `fullsky` layer compiles its own .hlsl asynchronously. A `sprites` layer that names no shader
// gets the one built into the plugin and is ready at once - and is otherwise exactly the same kind
// of object, which is the point: one type, one parameter list, one panel, one settings file, and no
// second mechanism for the layers that happen not to be the sky itself.
//
// Returns null when the manifest has no such layer, when its kind is not one this build draws, or
// when its shader is missing. Calling this again for the same package and layer reuses the existing
// program object rather than making a second one: the draw path, the shader cache and the overlay
// all hold pointers into it.
sky_program* load_package_layer(const std::shared_ptr<shared::loaded_sky>& sky, std::string_view layer_id);

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
} // namespace tw::skybox
