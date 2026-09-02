#include "pch.hxx"

#include "skybox/sky_program.hxx"

#include "plugin/diagnostics.hxx"

#include "resource/resource.hxx"

#include "skybox/sky_bytecode.hxx"
#include "skybox/sky_compile.hxx"
#include "skybox/sky_settings.hxx"
#include "skybox/sky_sprites.hxx"
#include "skybox/skybox_config.hxx"

namespace
{
constexpr std::string_view k_vertex_key = "shaders/sky_cube.vs.fxo";

// Packed as text so that a shader on disk can #include it, and read here so its annotation block can
// offer the palette knobs to every program that uses the palette.
constexpr std::string_view k_shared_header_key = "shaders/sky_common.hlsli";

constexpr std::size_t k_palette_floats = 24;

// Palettes, one register per line, matching assets/shaders/sky_common.hlsli:
//
//   c0 zenith rgb | c1 horizon rgb | c2 ground rgb
//   c3 light direction xyz (unit length, sky space) + w = cos of its angular radius
//   c4 light colour rgb + w = glow exponent
//   c5 runtime: x = seconds, y = program-specific
//
// Direction vectors are written out already normalised - the shader does not renormalise them, and
// a slightly long one would quietly widen the disc.
//
// clang-format off
constexpr std::array<float, k_palette_floats> k_day_constants { {
    0.10f, 0.28f, 0.62f, 0.f,
    0.78f, 0.80f, 0.78f, 0.f,
    0.10f, 0.11f, 0.13f, 0.f,
    0.34922f, 0.41906f, 0.83812f, 0.9998f,
    1.00f, 0.95f, 0.85f, 320.f,
    0.f, 1.f, 0.f, 0.f,
} };

constexpr std::array<float, k_palette_floats> k_night_constants { {
    0.010f, 0.015f, 0.045f, 0.f,
    0.060f, 0.080f, 0.140f, 0.f,
    0.010f, 0.010f, 0.020f, 0.f,
    -0.45113f, 0.55138f, 0.70176f, 0.9996f,
    0.85f, 0.88f, 1.00f, 90.f,
    0.f, 0.f, 0.f, 0.f,
} };
// clang-format on

struct builtin_def {
    std::string_view id;
    std::string_view display_name;
    std::string_view pixel_key;
    std::string_view source_key;
    std::span<const float> constants;
    bool markers_in_runtime_y;
};

// Order is the order the overlay lists them in, so the two that are meant to be looked at come
// before the diagnostic. The probe shares the day palette so that what stands out about it is the
// markers rather than a change of colour.
constexpr std::array<builtin_def, 3> k_builtins { {
    { "gradient", "Gradient Day", "shaders/sky_gradient.ps.fxo", "shaders/sky_gradient.ps.hlsl", k_day_constants, false },
    { "night", "Starry Night", "shaders/sky_night.ps.fxo", "shaders/sky_night.ps.hlsl", k_night_constants, false },
    { "probe", "Axis Probe (diagnostic)", "shaders/sky_probe.ps.fxo", "shaders/sky_probe.ps.hlsl", k_day_constants, true },
} };

// unique_ptr rather than the objects themselves: the draw path, the shader cache and the overlay
// all hold raw pointers into this, and a vector that reallocates would invalidate every one of them
// the moment a user drops a new .hlsl into the folder.
std::vector<std::unique_ptr<tw::skybox::sky_program>> g_owned;
std::vector<tw::skybox::sky_program*> g_programs;
std::span<const std::byte> g_vertex_bytecode;
bool g_initialized = false;

std::string_view packed_text(std::string_view key)
{
    const auto packed = tw::resource::get_resource(tw::resource::type::text, key);

    return packed.has_value() ? packed->text() : std::string_view {};
}

std::span<const std::byte> packed_bytecode(std::string_view key)
{
    const auto packed = tw::resource::get_resource(tw::resource::type::shader, key);
    if(!packed.has_value()) {
        TW_LOG_ERROR("sky_program: shader resource '{}' not found - was it built? (fxc step in src/resource/CMakeLists.txt)", key);
        return {};
    }

    return packed->bytes;
}

// Reads back whatever the user last moved this program's parameters to.
//
// Values live in the sky's own settings file rather than in the shader, so an edit to the .hlsl does
// not undo somebody's tuning - and a parameter the file has never seen simply keeps the default its
// annotation gave it.
void restore_saved_values(tw::skybox::sky_program& program)
{
    tw::skybox::settings::store saved;
    saved.load(tw::skybox::settings::path_for(program.settings_stem()));

    for(tw::skybox::sky_param& param : program.params) {
        // Leaves `param.value` at its default when the file has never seen this knob, which is what
        // lookup returning false means.
        static_cast<void>(saved.lookup(param.settings_layer, param.settings_id, param.value));

        // A shared knob's value belongs to the sky, so it has to land there rather than only on the
        // copy this panel row draws - whether it came from the file or from the manifest's default.
        if(param.is_shared() && program.sky != nullptr) {
            program.sky->values.write(param.shared_key, std::span<const float> { param.value.data(), static_cast<std::size_t>(param.count) });
        }
    }
}

// Everything a compiled shader says about itself, applied to the program: which registers may be
// written, what the annotation block calls them, and where the palette ends up.
void adopt_bytecode(tw::skybox::sky_program& program, std::string_view source)
{
    const tw::skybox::bytecode::reflection reflection = tw::skybox::bytecode::reflect(program.pixel_bytecode);

    // Taken whole. There is nothing to mask against any more: the block is the size of the register
    // file, so every register the shader can legally declare is one this program can hold.
    program.constant_runs = reflection.runs;

    // The shared header first, the program's own block second. sky_common.hlsli offers a knob for
    // every palette variable, and each one survives only if this shader actually reads it - fxc
    // drops what a program does not use, so the constant table answers "does this apply here?" on
    // its own. A program that names the same variable again wins, hence the de-duplication below.
    tw::skybox::param_block shared = tw::skybox::parse_shared_params(packed_text(k_shared_header_key), reflection);
    tw::skybox::param_block block = tw::skybox::parse_params(source, reflection);

    if(!block.display_name.empty()) {
        program.display_name = std::move(block.display_name);
    }

    // Before restore_saved_value below, which reads through it.
    program.param_version = block.version;

    program.params = std::move(shared.params);

    // A shared knob starts where *this program's* palette already is, not at the example value in
    // the header - Starry Night is not Gradient Day, and a header cannot know which program it is
    // talking to. Its defaults are shape, not data.
    for(tw::skybox::sky_param& param : program.params) {
        for(int i = 0; i < param.count; ++i) {
            const auto slot = static_cast<std::size_t>(param.reg * 4 + param.component + i);
            if(slot < program.constants.size()) {
                param.value[static_cast<std::size_t>(i)] = program.constants[slot];
            }
        }

        param.default_value = param.value;
    }

    for(tw::skybox::sky_param& own : block.params) {
        const auto same = std::find_if(program.params.begin(), program.params.end(), [&own](const tw::skybox::sky_param& existing) {
            return existing.key == own.key;
        });

        if(same == program.params.end()) {
            program.params.push_back(std::move(own));
        }
        else {
            *same = std::move(own);
        }
    }

    tw::skybox::order_by_group(program.params);

    // A sky with no manifest is a sky of one layer that does not say so, and its settings are keyed
    // the same way a package's are. The version namespaces the ids rather than the file, because a
    // rewritten shader is still the same sky - it is only its knobs that stopped meaning what they
    // used to, which is exactly what `version =` is for.
    const std::string prefix = program.param_version == 0 ? std::string {} : ("v" + std::to_string(program.param_version) + ".");

    for(tw::skybox::sky_param& param : program.params) {
        param.settings_layer = "sky";
        param.settings_id = prefix + param.key;
    }

    restore_saved_values(program);

    // Last, once the list is final: anything caching per-parameter state keyed on this counter must
    // not see the new generation while the vector is still being assembled.
    ++program.params_generation;

    tw::skybox::refresh_constants(program);
}

// The package equivalent of adopt_bytecode: the same sky_param list, read from a manifest instead of
// from a comment inside the shader.
//
// Deliberately not a second kind of program. Everything downstream - the panel, the shader cache,
// the renderer, the reload poll - keeps working on one type, and what differs is only where the
// declarations came from and where their values are saved. A parallel type is exactly the second
// parameter system this format exists to remove.
void adopt_manifest(tw::skybox::sky_program& program, const tw::skybox::package::manifest& sky, const tw::skybox::package::layer& layer)
{
    const tw::skybox::bytecode::reflection reflection = tw::skybox::bytecode::reflect(program.pixel_bytecode);
    program.constant_runs = reflection.runs;

    // The vertex stage too, when this layer brought one. A name in the manifest resolves in whichever
    // stage declares it, so how a layer's sprites *move* is nameable exactly like how they are lit -
    // rather than being a fixed register the author has to know about and nobody can see.
    const tw::skybox::bytecode::reflection vertex_reflection = tw::skybox::bytecode::reflect(program.vertex_code);
    program.vertex_constant_runs = vertex_reflection.runs;

    program.vertex_time_register = -1;
    if(tw::skybox::sky_param time; tw::skybox::resolve_variable(vertex_reflection, "g_time.x", 1, time)) {
        program.vertex_time_register = time.reg;
        program.vertex_time_component = time.component;
    }

    program.display_name = sky.name;
    program.package_layer = layer.id;
    program.param_version = sky.version;

    program.params.clear();
    program.bindings.clear();

    // The knobs the sky owns rather than this layer: its lights, and any loose shared value. They
    // head the panel the way they head the manifest, and their settings live under a layer id of
    // their own - moving a light between layers must not lose its value, and it cannot, because it
    // never belonged to one.
    //
    // Only one layer lists them, or every light would get a slider per layer that reads it. Which
    // one is decided when the sky is loaded and does not change under a recompile.
    if(program.sky != nullptr && program.sky->shared_owner == layer.id) {
        for(const tw::skybox::shared::knob& source : program.sky->values.knobs()) {
            tw::skybox::sky_param param;

            // No register: this value does not live in any layer's constant block. It reaches the
            // layers that want it through their bindings, which is the entire point.
            param.shared_key = source.key;
            param.key = source.key;
            param.widget_id = "##" + source.key;
            param.label = source.label;
            param.group = source.group;
            param.count = source.count;
            param.value = source.value;
            param.default_value = source.default_value;
            param.min_value = source.min_value;
            param.max_value = source.max_value;
            param.settings_layer = "shared";
            param.settings_id = source.key;

            program.params.push_back(std::move(param));
        }
    }

    for(const tw::skybox::package::param& source : layer.params) {
        tw::skybox::sky_param param;

        if(!source.property.empty()) {
            // A property of the layer kind, not of its shader. Nothing to resolve - the name is the
            // whole address, and whoever draws this kind of layer is what understands it.
            param.property = source.property;
        }
        else if(tw::skybox::resolve_variable(reflection, source.variable, source.count, param)) {
            // Found in the pixel stage. Tried first because that is where almost everything lives.
        }
        else if(tw::skybox::resolve_variable(vertex_reflection, source.variable, source.count, param)) {
            param.vertex_stage = true;
        }
        else {
            // A manifest naming something neither shader declares is worth saying out loud, unlike
            // the shared-header case where it is the normal outcome: here the author wrote this line
            // about this shader, so an unresolved name is a mistake rather than a non-answer.
            TW_LOG_WARNING("sky_program: '{}' layer '{}': '{}' is not a constant either stage declares - parameter ignored",
                sky.name,
                layer.id,
                source.variable);
            continue;
        }

        param.label = source.label;
        param.group = source.group;
        param.count = source.count;
        param.value = source.value;
        param.default_value = source.default_value;
        param.min_value = source.min_value;
        param.max_value = source.max_value;
        param.settings_layer = layer.id;
        param.settings_id = source.id;

        // Unique across the whole sky, not just across one shader. The panel draws every layer of a
        // package in one strip, and two layers both keying a knob "c0x" would hand ImGui the same id
        // twice - two sliders that move together.
        param.key = layer.id + "." + source.id;
        param.widget_id = "##" + param.key;

        program.params.push_back(std::move(param));
    }

    // The layer's bindings, resolved against its own shader. A source that names nothing is left in
    // the list with no register, so it costs nothing at apply time and still shows up in the log.
    for(const tw::skybox::package::binding& source : layer.bindings) {
        tw::skybox::resolved_binding binding;
        binding.source = source.source;

        if(!tw::skybox::resolve_binding_target(reflection, source.variable, binding)) {
            if(!tw::skybox::resolve_binding_target(vertex_reflection, source.variable, binding)) {
                TW_LOG_WARNING("sky_program: '{}' layer '{}': cannot bind to '{}' - neither stage declares such a variable",
                    sky.name,
                    layer.id,
                    source.variable);
                continue;
            }

            binding.vertex_stage = true;
        }

        program.bindings.push_back(std::move(binding));
    }

    tw::skybox::order_by_group(program.params);

    // Whatever the user has already moved, from this sky's own settings file - the same call the
    // lone-.hlsl path makes, because there is one way a sky keeps its settings.
    restore_saved_values(program);

    ++program.params_generation;

    tw::skybox::refresh_constants(program);
}

tw::skybox::sky_program& add(std::unique_ptr<tw::skybox::sky_program> program)
{
    g_owned.push_back(std::move(program));
    g_programs.push_back(g_owned.back().get());

    return *g_owned.back();
}

// Normalised so the same file reached by two different spellings is one program rather than two.
std::string path_id(const std::filesystem::path& path)
{
    std::error_code ec;
    const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, ec);

    return (ec ? path : canonical).string();
}

// Applies a compile result to a program, and says whether new bytecode was installed. Kept in one
// place because the first compile and every reload have to agree about what happens on failure:
// keep the previous bytecode, and let the diagnostics be what changes.
bool apply_compile(tw::skybox::sky_program& program, tw::skybox::compile::result&& compiled)
{
    program.diagnostics = std::move(compiled.diagnostics);

    if(!compiled.ok()) {
        TW_LOG_ERROR("sky_program: '{}' did not compile: {}", program.id, program.diagnostics);
        return false;
    }

    program.owned_bytecode = std::move(compiled.bytecode);
    program.pixel_bytecode = program.owned_bytecode;

    // The one branch between a package layer and a lone shader, and it is only about where the
    // declarations come from. Everything after this point is identical for both.
    const tw::skybox::package::layer* layer = program.package_layer_ref();
    if(layer != nullptr) {
        adopt_manifest(program, *program.sky->sky, *layer);
    }
    else {
        adopt_bytecode(program, compiled.source);
    }

    if(!program.diagnostics.empty()) {
        TW_LOG_WARNING("sky_program: '{}' compiled with warnings: {}", program.id, program.diagnostics);
    }

    TW_LOG_INFO("sky_program: '{}' compiled, {} bytes, constant registers {}",
        program.id,
        program.owned_bytecode.size(),
        tw::skybox::bytecode::describe(program.constant_runs));

    return true;
}
} // namespace

namespace tw::skybox
{
void initialize_programs()
{
    if(g_initialized) {
        return;
    }
    g_initialized = true;

    g_vertex_bytecode = packed_bytecode(k_vertex_key);

    for(const builtin_def& def : k_builtins) {
        auto program = std::make_unique<sky_program>();

        program->id.assign(def.id);
        program->display_name.assign(def.display_name);
        program->pixel_bytecode = packed_bytecode(def.pixel_key);
        program->markers_in_runtime_y = def.markers_in_runtime_y;

        std::copy(def.constants.begin(), def.constants.end(), program->constants.begin());

        // The source is packed alongside the bytecode purely so a built-in can carry the same
        // annotation block a user's shader does. Nothing else reads it.
        adopt_bytecode(*program, packed_text(def.source_key));

        if(!program->usable()) {
            program->diagnostics = "the compiled shader is missing from this build";
        }

        TW_LOG_INFO("sky_program: built-in '{}' ready, constant registers {}, {} parameter(s)",
            program->id,
            tw::skybox::bytecode::describe(program->constant_runs),
            program->params.size());

        add(std::move(program));
    }
}

std::span<sky_program* const> programs() noexcept
{
    return g_programs;
}

sky_program* find_program(std::string_view id) noexcept
{
    if(id.empty()) {
        return nullptr;
    }

    for(sky_program* program : g_programs) {
        if(program->id == id) {
            return program;
        }
    }

    return nullptr;
}

void refresh_constants(sky_program& program) noexcept
{
    apply_params(program.params, program.constants, program.vertex_constants);

    // Bindings last, and that order is the rule rather than an accident: a bound variable is the
    // sky's answer, and a layer's own parameter must not be able to overwrite it. If both name the
    // same register the manifest is contradicting itself, and the shared value is the one that keeps
    // the layers agreeing with each other.
    if(program.sky != nullptr) {
        shared::apply_bindings(program.sky->values, program.bindings, program.constants, program.vertex_constants);
    }
}

std::span<const std::byte> vertex_bytecode() noexcept
{
    return g_vertex_bytecode;
}

sky_program* load_file_program(const std::filesystem::path& path)
{
    const std::string id = path_id(path);

    if(sky_program* existing = find_program(id); existing != nullptr) {
        return existing;
    }

    auto program = std::make_unique<sky_program>();

    program->id = id;
    program->display_name = path.stem().string();
    std::copy(k_day_constants.begin(), k_day_constants.end(), program->constants.begin());
    program->source_path = path;

    std::error_code ec;
    program->source_time = std::filesystem::last_write_time(path, ec);

    // Started, not waited on. Selecting a heavy shader used to freeze the game for as long as it
    // took to compile; now the game keeps its own sky for those few seconds and the overlay says
    // what is happening.
    program->pending = compile::shader_file_async(path, compile::stage::pixel);

    return &add(std::move(program));
}

sky_program* load_package_layer(const std::shared_ptr<shared::loaded_sky>& sky, std::string_view layer_id)
{
    if(sky == nullptr || sky->sky == nullptr) {
        return nullptr;
    }

    const package::manifest& manifest = *sky->sky;

    const package::layer* layer = manifest.find_layer(layer_id);
    if(layer == nullptr) {
        return nullptr;
    }

    if(layer->kind != package::layer_kind::fullsky && layer->kind != package::layer_kind::sprites) {
        return nullptr;
    }

    // A `sprites` layer is a *pair* of shaders - "<stem>.vs.hlsl" and "<stem>.ps.hlsl" - because its
    // geometry is its own: quads with a billboard basis baked in, not the cube every fullsky layer
    // paints. A `fullsky` layer names one file and borrows the shared cube vertex shader.
    const bool pair = layer->kind == package::layer_kind::sprites;

    // Naming no shader at all falls back to the one built into the plugin. Kept as a fallback rather
    // than as the only way, because a sky whose clouds come from nowhere visible is a sky whose
    // author cannot change them - and it is not a special case in the code either: the same program,
    // with its bytecode read from a resource instead of from a compile, exactly as the three
    // built-in skies have always worked.
    const bool builtin_shader = pair && layer->shader.empty();

    std::filesystem::path source;
    std::filesystem::path vertex_source;
    std::error_code ec;

    if(!builtin_shader) {
        source = manifest.root / (pair ? layer->shader + ".ps.hlsl" : layer->shader);

        if(!std::filesystem::is_regular_file(source, ec)) {
            TW_LOG_ERROR("sky_program: '{}' layer '{}': no shader at '{}'", manifest.name, layer->id, source.string());
            return nullptr;
        }

        if(pair) {
            vertex_source = manifest.root / (layer->shader + ".vs.hlsl");

            if(!std::filesystem::is_regular_file(vertex_source, ec)) {
                TW_LOG_ERROR("sky_program: '{}' layer '{}': no vertex shader at '{}' - a sprites layer needs both halves",
                    manifest.name,
                    layer->id,
                    vertex_source.string());
                return nullptr;
            }
        }
    }

    // Keyed by the package and the layer rather than by the shader's path, because two packages may
    // perfectly well ship the same shader file name, and because this is the id the catalog and the
    // settings file both mean when they say which sky is selected.
    const std::string id = sky->stem + "#" + std::string { layer_id };

    sky_program* program = find_program(id);
    if(program == nullptr) {
        auto created = std::make_unique<sky_program>();
        created->id = id;

        // The palette is the fullsky contract - c0-c5 as sky_common.hlsli describes them - and means
        // nothing to any other kind of layer. Seeding a sprite layer with it would leave a sky
        // colour sitting in whatever register its own shader happens to put a knob at, visible for
        // exactly as long as it takes the author to forget one binding.
        if(layer->kind == package::layer_kind::fullsky) {
            std::copy(k_day_constants.begin(), k_day_constants.end(), created->constants.begin());
        }

        program = &add(std::move(created));
    }

    // The first layer loaded for a sky lists its shared knobs in the panel. Recorded on the sky
    // rather than decided per rebuild: a recompile must not hand the job to a different layer and
    // leave every light drawn twice, or not at all.
    if(sky->shared_owner.empty()) {
        sky->shared_owner.assign(layer_id);
    }

    program->sky = sky;
    program->package_layer.assign(layer_id);
    program->display_name = manifest.name;
    program->source_path = source;
    program->vertex_source_path = vertex_source;

    if(builtin_shader) {
        program->owned_bytecode.clear();
        program->owned_vertex_bytecode.clear();
        program->pixel_bytecode = packed_bytecode(sprites::k_builtin_pixel_key);
        program->vertex_code = packed_bytecode(sprites::k_builtin_vertex_key);

        // Nothing to wait for, so the parameters and bindings are adopted here rather than by the
        // compile poll. Same function, same list, same order - only the trigger differs.
        adopt_manifest(*program, manifest, *layer);

        return program;
    }

    program->source_time = std::filesystem::last_write_time(source, ec);

    // Started, not waited on - the game keeps its own sky for the few seconds a heavy shader takes,
    // and the overlay says what is happening. Same contract as the lone-file path.
    program->pending = compile::shader_file_async(source, compile::stage::pixel);

    if(!vertex_source.empty()) {
        program->vertex_source_time = std::filesystem::last_write_time(vertex_source, ec);
        program->pending_vertex = compile::shader_file_async(vertex_source, compile::stage::vertex);
    }

    return program;
}

program_event poll_program(sky_program& program, bool watch)
{
    if(!program.from_file()) {
        return program_event::none;
    }

    // The vertex half first, and only reported when it fails.
    //
    // Both halves are collected before either is announced because they are one program: a layer
    // whose vertex stage landed and whose pixel stage has not is a layer that must not draw yet, and
    // "compiled" said twice would have the overlay claim a reload that is half done.
    if(program.pending_vertex.valid()) {
        std::optional<compile::result> collected = program.pending_vertex.take();
        if(!collected.has_value()) {
            return program_event::none;
        }

        // take() empties the handle, which is also how compiling() stops being true.
        compile::result finished = std::move(*collected);

        if(!finished.ok()) {
            program.diagnostics = std::move(finished.diagnostics);
            TW_LOG_ERROR("sky_program: '{}' vertex stage did not compile: {}", program.id, program.diagnostics);
            return program_event::compile_failed;
        }

        program.owned_vertex_bytecode = std::move(finished.bytecode);
        program.vertex_code = program.owned_vertex_bytecode;
    }

    if(program.pending.valid()) {
        std::optional<compile::result> collected = program.pending.take();
        if(!collected.has_value()) {
            return program_event::none;
        }

        return apply_compile(program, std::move(*collected)) ? program_event::compiled : program_event::compile_failed;
    }

    if(!watch) {
        return program_event::none;
    }

    std::error_code ec;

    // The vertex source is watched on its own: a cloud whose shape is being reworked has to reload
    // without anyone having to touch the file that shades it.
    if(!program.vertex_source_path.empty()) {
        const std::filesystem::file_time_type stamp = std::filesystem::last_write_time(program.vertex_source_path, ec);
        if(!ec && stamp != program.vertex_source_time) {
            program.vertex_source_time = stamp;
            program.pending_vertex = compile::shader_file_async(program.vertex_source_path, compile::stage::vertex);

            // The pixel half too, because installing new vertex bytecode goes through apply_compile
            // - it is what rebuilds the parameters and invalidates the device shaders, and there is
            // no half of it worth splitting out to save one compile of a file that has not changed.
            program.pending = compile::shader_file_async(program.source_path, compile::stage::pixel);

            return program_event::compile_started;
        }
    }

    const std::filesystem::file_time_type now = std::filesystem::last_write_time(program.source_path, ec);
    if(ec || now == program.source_time) {
        return program_event::none;
    }

    // Stamped before the compile rather than after it: a file that does not compile must not be
    // retried on every poll, and the next edit moves the timestamp again anyway.
    program.source_time = now;
    program.pending = compile::shader_file_async(program.source_path, compile::stage::pixel);

    return program_event::compile_started;
}
} // namespace tw::skybox
