#pragma once

// What a compiled D3D9 shader says about its own constants.
//
// Two things are read out of the `CTAB` block fxc embeds: which float registers the shader declares,
// and what it calls them.
//
// Which registers matters because a shader model 1-3 shader carries its own literals inside the
// bytecode, as `def cN, ...` instructions, in the *same* register file the application writes. They
// are loaded when the shader is bound, so uploading over one does not misconfigure the program - it
// corrupts it. Uploading only what the shader declares is the way to never do that.
//
// The names matter because they are what a parameter refers to. A shader's author writes
// `param=g_eclipse.x` and means the variable, not a register number - register numbers are fxc's to
// assign and they move the moment anything above them is edited.
//
// The alternative to reading any of this - "everything below the lowest def is safe" - is true but
// far too weak: a shader that reads one uniform at c5 and packs its literals from c0 is entirely
// ordinary, and that rule would refuse to upload anything at all. It survives here only as the
// fallback for bytecode with no constant table.
//
// This used to report a fixed-width bitmask, and the width was the problem rather than the idea. A
// `uint32` capped the reader at c31 for no reason ps_3_0 cares about - it offers 224 - and the same
// number then had to be repeated wherever the block was sized or uploaded. It was repeated wrongly
// twice, and both times the symptom was a shader that compiled, drew, and quietly lost whole layers
// because their constants never arrived. Runs carry the same information with nothing to truncate
// and no ceiling to keep in step.
namespace tw::skybox::bytecode
{
// The float constant register file a ps_3_0 shader has, fixed by the shader model rather than by the
// card - which is why D3DCAPS9 has no pixel-shader equivalent of MaxVertexShaderConst to ask. This
// is the one genuine limit in the chain, and the only number here that comes from outside.
inline constexpr int k_float_registers = 224;

struct constant {
    std::string name;
    int reg {};   // first register
    int count {}; // registers occupied - 1 for a float4, 4 for a float4x4
};

// A contiguous stretch of registers that may be written, in ascending order and already merged, so
// the upload is one SetPixelShaderConstantF per run and the gaps between them - which belong to the
// shader's own literals - are never touched.
struct register_run {
    int first {};
    int count {};
};

struct reflection {
    // Every register the shader declares, as runs. Empty means "declare nothing, upload nothing",
    // which is the safe answer and not an error.
    std::vector<register_run> runs;

    // Highest register named, or -1 when nothing was. Kept because it answers a question the runs
    // answer awkwardly, and because it is what a caller sizes or checks against.
    int highest_register { -1 };

    // Empty when the shader carries no constant table, in which case `runs` came from the fallback
    // above and there are no names to be had.
    std::vector<constant> floats;

    // Whether register `reg` falls inside one of the runs - that is, whether writing it is safe.
    [[nodiscard]] bool declares(int reg) const noexcept
    {
        for(const register_run& run : runs) {
            if(reg >= run.first && reg < run.first + run.count) {
                return true;
            }
        }
        return false;
    }
};

[[nodiscard]] reflection reflect(std::span<const std::byte> shader);

// "c0-c2, c5, c6-c25" - for the log, where the alternative was a hex mask nobody can read at a
// glance and which stopped being a single integer anyway.
[[nodiscard]] std::string describe(std::span<const register_run> runs);
} // namespace tw::skybox::bytecode
