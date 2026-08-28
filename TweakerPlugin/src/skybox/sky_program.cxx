#include "pch.hxx"

#include "skybox/sky_program.hxx"

#include "plugin/diagnostics.hxx"

#include "resource/resource.hxx"

#include "skybox/sky_bytecode.hxx"
#include "skybox/sky_compile.hxx"
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

// Restores whatever the user last moved this parameter to. Values live in the settings file rather
// than in the shader, so an edit to the .hlsl does not undo somebody's tuning - and a parameter the
// file has never seen simply keeps the default its annotation gave it.
void restore_saved_value(const tw::skybox::sky_program& program, tw::skybox::sky_param& param)
{
    const std::string_view saved = tw::skybox::config::param_value(tw::skybox::param_storage_key(program, param));
    if(saved.empty()) {
        return;
    }

    std::size_t start = 0;
    for(int i = 0; i < param.count && start <= saved.size(); ++i) {
        const std::size_t comma = saved.find(',', start);
        const std::string_view field = saved.substr(start, comma == std::string_view::npos ? std::string_view::npos : comma - start);

        float value = param.value[static_cast<std::size_t>(i)];
        std::from_chars(field.data(), field.data() + field.size(), value);
        param.value[static_cast<std::size_t>(i)] = value;

        if(comma == std::string_view::npos) {
            break;
        }
        start = comma + 1;
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

    for(tw::skybox::sky_param& param : program.params) {
        restore_saved_value(program, param);
    }

    // Last, once the list is final: anything caching per-parameter state keyed on this counter must
    // not see the new generation while the vector is still being assembled.
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

    adopt_bytecode(program, compiled.source);

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
    apply_params(program.params, program.constants);
}

std::string param_storage_key(const sky_program& program, const sky_param& param)
{
    // Version 0 keeps the original key shape, so no existing setting is orphaned by this gaining a
    // version at all - only a shader that opts in gets a new namespace.
    if(program.param_version == 0) {
        return "param." + program.display_name + "." + param.key;
    }

    return "param." + program.display_name + ".v" + std::to_string(program.param_version) + "." + param.key;
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
    program->pending = compile::pixel_shader_file_async(path);

    return &add(std::move(program));
}

program_event poll_program(sky_program& program, bool watch)
{
    if(!program.from_file()) {
        return program_event::none;
    }

    if(program.pending.valid()) {
        if(program.pending.wait_for(std::chrono::seconds { 0 }) != std::future_status::ready) {
            return program_event::none;
        }

        // get() empties the future, which is also how compiling() stops being true.
        compile::result finished = program.pending.get();

        return apply_compile(program, std::move(finished)) ? program_event::compiled : program_event::compile_failed;
    }

    if(!watch) {
        return program_event::none;
    }

    std::error_code ec;
    const std::filesystem::file_time_type now = std::filesystem::last_write_time(program.source_path, ec);
    if(ec || now == program.source_time) {
        return program_event::none;
    }

    // Stamped before the compile rather than after it: a file that does not compile must not be
    // retried on every poll, and the next edit moves the timestamp again anyway.
    program.source_time = now;
    program.pending = compile::pixel_shader_file_async(program.source_path);

    return program_event::compile_started;
}
} // namespace tw::skybox
