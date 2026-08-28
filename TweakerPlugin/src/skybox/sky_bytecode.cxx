#include "pch.hxx"

#include "skybox/sky_bytecode.hxx"

#include "plugin/diagnostics.hxx"

namespace
{
// Shader model 1-3 token stream, as documented for the D3D9 bytecode format:
//
//   [0]      version token: low word = minor | major << 8, high word 0xFFFF (ps) or 0xFFFE (vs)
//   [1..]    instruction tokens, terminated by 0x0000FFFF
//
// An instruction token carries its opcode in the low 16 bits and, from shader model 2 on, the
// number of DWORDs that follow it in bits 24-27. Comment tokens are the exception: their length is
// 15 bits wide and lives at bit 16, because a comment can hold a whole constant table - which is
// exactly what this file is here to read.
constexpr DWORD k_end_token = 0x0000FFFF;
constexpr DWORD k_comment_opcode = 0xFFFE;
constexpr DWORD k_def_opcode = 0x51;

constexpr DWORD k_ctab_fourcc = 0x42415443; // 'CTAB', little endian

// D3DXSHADER_CONSTANTTABLE: seven DWORDs, all offsets relative to the start of the structure (which
// is the DWORD after the FourCC).
constexpr std::size_t k_table_dwords = 7;
constexpr std::size_t k_table_constants_index = 3;
constexpr std::size_t k_table_constant_info_index = 4;

// D3DXSHADER_CONSTANTINFO: Name(4) RegisterSet(2) RegisterIndex(2) RegisterCount(2) Reserved(2)
// TypeInfo(4) DefaultValue(4).
constexpr std::size_t k_info_stride = 20;
constexpr std::size_t k_info_name = 0;
constexpr std::size_t k_info_register_set = 4;
constexpr std::size_t k_info_register_index = 6;
constexpr std::size_t k_info_register_count = 8;

// D3DXRS_FLOAT4. The bool, int4 and sampler register sets live in their own files and cannot
// collide with what SetPixelShaderConstantF writes.
constexpr std::uint16_t k_register_set_float4 = 2;

constexpr std::uint32_t k_register_number_mask = 0x7FF;
constexpr int k_max_register = tw::skybox::bytecode::k_float_registers;

std::uint16_t read_word(const std::byte* base, std::size_t offset) noexcept
{
    std::uint16_t value = 0;
    std::memcpy(&value, base + offset, sizeof(value));
    return value;
}

std::uint32_t read_dword(const std::byte* base, std::size_t offset) noexcept
{
    std::uint32_t value = 0;
    std::memcpy(&value, base + offset, sizeof(value));
    return value;
}

// Names are NUL-terminated and live inside the same block, so the terminator has to be found rather
// than trusted - a truncated or hostile table must not walk off the end.
std::string read_string(const std::byte* base, std::size_t table_bytes, std::size_t offset)
{
    if(offset >= table_bytes) {
        return {};
    }

    const auto* chars = reinterpret_cast<const char*>(base);
    std::size_t end = offset;
    while(end < table_bytes && chars[end] != '\0') {
        ++end;
    }

    return std::string { chars + offset, end - offset };
}

// Sorted, then coalesced. Two constants can share a register only through a malformed table, but
// adjacent ones are the normal case - c0, c1, c2 as three float3 palette entries is three table rows
// and wants to be one upload.
void merge_runs(std::vector<tw::skybox::bytecode::register_run>& runs)
{
    std::sort(runs.begin(), runs.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });

    std::vector<tw::skybox::bytecode::register_run> merged;
    merged.reserve(runs.size());

    for(const tw::skybox::bytecode::register_run& run : runs) {
        if(!merged.empty() && run.first <= merged.back().first + merged.back().count) {
            const int end = (std::max)(merged.back().first + merged.back().count, run.first + run.count);
            merged.back().count = end - merged.back().first;
            continue;
        }

        merged.push_back(run);
    }

    runs = std::move(merged);
}

void read_constant_table(const DWORD* table, std::size_t table_dwords, tw::skybox::bytecode::reflection& out)
{
    if(table_dwords < k_table_dwords) {
        return;
    }

    const auto* base = reinterpret_cast<const std::byte*>(table);
    const std::size_t table_bytes = table_dwords * sizeof(DWORD);

    const std::size_t constants = table[k_table_constants_index];
    const std::size_t info_offset = table[k_table_constant_info_index];

    for(std::size_t i = 0; i < constants; ++i) {
        const std::size_t offset = info_offset + i * k_info_stride;
        if(offset + k_info_stride > table_bytes) {
            break;
        }

        if(read_word(base, offset + k_info_register_set) != k_register_set_float4) {
            continue;
        }

        const int first = read_word(base, offset + k_info_register_index);
        const int count = read_word(base, offset + k_info_register_count);

        std::string name = read_string(base, table_bytes, read_dword(base, offset + k_info_name));

        // A malformed table is the only way these can be out of range, and the register file is the
        // only bound there is - so clamp to it rather than to a number of this project's choosing.
        if(first < 0 || count <= 0 || first + count > k_max_register) {
            TW_LOG_WARNING(
                "sky_bytecode: '{}' claims c{}..c{}, outside the ps_3_0 register file - entry ignored", name, first, first + count - 1);
            continue;
        }

        out.highest_register = (std::max)(out.highest_register, first + count - 1);

        out.runs.push_back(tw::skybox::bytecode::register_run { first, count });
        out.floats.push_back(tw::skybox::bytecode::constant { std::move(name), first, count });
    }

    merge_runs(out.runs);
}

// Fallback for bytecode with no constant table: every register below the shader's lowest literal.
// Correct in the sense that it can never overwrite one, and weak in the sense described in the
// header - which is why it is only reached when there is nothing better to read.
//
// No literals *and* no table means a shader with no constants this reader can see. Writing nothing
// is then both safe and correct: there is nothing to configure and nothing to corrupt.
std::vector<tw::skybox::bytecode::register_run> runs_below_lowest_def(int lowest_def)
{
    if(lowest_def < 0) {
        return {};
    }
    if(lowest_def == 0) {
        return {};
    }

    return { tw::skybox::bytecode::register_run { 0, (std::min)(lowest_def, k_max_register) } };
}
} // namespace

namespace tw::skybox::bytecode
{
reflection reflect(std::span<const std::byte> shader)
{
    reflection out;

    if(shader.size() < 8 || (shader.size() % sizeof(DWORD)) != 0) {
        return out;
    }

    if((reinterpret_cast<std::uintptr_t>(shader.data()) % alignof(DWORD)) != 0) {
        return out;
    }

    const DWORD* tokens = reinterpret_cast<const DWORD*>(shader.data());
    const std::size_t count = shader.size() / sizeof(DWORD);

    // Instruction lengths are only encoded from shader model 2 onwards; walking a 1.x stream would
    // be guesswork, and this project never produces one.
    if(((tokens[0] >> 8) & 0xFF) < 2) {
        return out;
    }

    int lowest_def = -1;

    for(std::size_t i = 1; i < count;) {
        const DWORD token = tokens[i];
        if(token == k_end_token) {
            break;
        }

        if((token & 0xFFFF) == k_comment_opcode) {
            const std::size_t length = (token >> 16) & 0x7FFF;
            if(i + 1 + length > count) {
                break;
            }

            // The constant table is the first thing in its comment, tagged with a FourCC.
            if(length >= 1 + k_table_dwords && tokens[i + 1] == k_ctab_fourcc) {
                read_constant_table(&tokens[i + 2], length - 1, out);
                return out;
            }

            i += length + 1;
            continue;
        }

        const std::size_t length = (token >> 24) & 0x0F;

        if((token & 0xFFFF) == k_def_opcode && i + 1 < count) {
            const int reg = static_cast<int>(tokens[i + 1] & k_register_number_mask);
            if(lowest_def < 0 || reg < lowest_def) {
                lowest_def = reg;
            }
        }

        i += length + 1;
    }

    // No constant table: the weak rule is all there is, and there are no names to go with it.
    out.runs = runs_below_lowest_def(lowest_def);

    return out;
}

std::string describe(std::span<const register_run> runs)
{
    if(runs.empty()) {
        return "none";
    }

    std::string out;
    for(const register_run& run : runs) {
        if(!out.empty()) {
            out += ", ";
        }

        out += "c" + std::to_string(run.first);
        if(run.count > 1) {
            out += "-c" + std::to_string(run.first + run.count - 1);
        }
    }

    return out;
}
} // namespace tw::skybox::bytecode
