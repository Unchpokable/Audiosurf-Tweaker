#include "pch.hxx"

#include "skybox/sky_caps.hxx"

#include "plugin/diagnostics.hxx"

namespace
{
bool g_reported = false;

std::string behavior_flag_names(DWORD flags)
{
    // Only the flags CreateDevice actually accepts, and only the ones whose presence or absence
    // changes what this plugin may do. The reversed CreateD3D passes exactly one of the first
    // three; anything else showing up here means the game was patched or wrapped (a d3d9.dll
    // shim, DXVK) and the conclusions in skybox-procedural.md need re-checking.
    struct named_flag {
        DWORD bit;
        const char* name;
    };

    constexpr std::array<named_flag, 8> k_flags { {
        { D3DCREATE_SOFTWARE_VERTEXPROCESSING, "SOFTWARE_VERTEXPROCESSING" },
        { D3DCREATE_HARDWARE_VERTEXPROCESSING, "HARDWARE_VERTEXPROCESSING" },
        { D3DCREATE_MIXED_VERTEXPROCESSING, "MIXED_VERTEXPROCESSING" },
        { D3DCREATE_PUREDEVICE, "PUREDEVICE" },
        { D3DCREATE_MULTITHREADED, "MULTITHREADED" },
        { D3DCREATE_FPU_PRESERVE, "FPU_PRESERVE" },
        { D3DCREATE_DISABLE_DRIVER_MANAGEMENT, "DISABLE_DRIVER_MANAGEMENT" },
        { D3DCREATE_ADAPTERGROUP_DEVICE, "ADAPTERGROUP_DEVICE" },
    } };

    std::string out;
    for(const auto& flag : k_flags) {
        if((flags & flag.bit) != 0) {
            if(!out.empty()) {
                out += " | ";
            }
            out += flag.name;
        }
    }

    return out.empty() ? std::string { "(none recognised)" } : out;
}

const char* yes_no(bool value) noexcept
{
    return value ? "yes" : "no";
}

// Whether the device would give us a cube map we can render into - the whole `bake` mode of the
// plan depends on this, and unlike the shader models it is a per-format question the driver
// answers rather than a caps bit.
void report_render_target_formats(IDirect3DDevice9* device, const D3DDEVICE_CREATION_PARAMETERS& params)
{
    IDirect3D9* d3d = nullptr;
    if(FAILED(device->GetDirect3D(&d3d)) || d3d == nullptr) {
        TW_LOG_WARNING("sky_caps: GetDirect3D failed - render target cube support unknown");
        return;
    }

    D3DDISPLAYMODE mode {};
    if(FAILED(d3d->GetAdapterDisplayMode(params.AdapterOrdinal, &mode))) {
        TW_LOG_WARNING("sky_caps: GetAdapterDisplayMode failed - render target cube support unknown");
        d3d->Release();
        return;
    }

    struct named_format {
        D3DFORMAT format;
        const char* name;
    };

    constexpr std::array<named_format, 3> k_formats { {
        { D3DFMT_A8R8G8B8, "A8R8G8B8" },
        { D3DFMT_A16B16G16R16F, "A16B16G16R16F" },
        { D3DFMT_A2B10G10R10, "A2B10G10R10" },
    } };

    for(const auto& entry : k_formats) {
        const HRESULT hr = d3d->CheckDeviceFormat(
            params.AdapterOrdinal, params.DeviceType, mode.Format, D3DUSAGE_RENDERTARGET, D3DRTYPE_CUBETEXTURE, entry.format);

        TW_LOG_INFO("sky_caps:   render target cube {}: {}", entry.name, SUCCEEDED(hr) ? "supported" : "no");
    }

    d3d->Release();
}

// Phase 2 wants to compile user-authored HLSL in-process. Nothing depends on it yet, but the
// answer costs one LoadLibrary and is worth having from the same run as everything else.
void report_compiler_availability()
{
    HMODULE compiler = ::GetModuleHandleA("d3dcompiler_47.dll");
    bool already_loaded = compiler != nullptr;

    if(compiler == nullptr) {
        compiler = ::LoadLibraryA("d3dcompiler_47.dll");
    }

    if(compiler == nullptr) {
        TW_LOG_INFO("sky_caps: d3dcompiler_47.dll not available - runtime HLSL compilation would need the D3DX9 fallback");
    }
    else {
        const bool has_entry = ::GetProcAddress(compiler, "D3DCompile") != nullptr;
        TW_LOG_INFO("sky_caps: d3dcompiler_47.dll present ({}), D3DCompile exported: {}",
            already_loaded ? "already loaded by the process" : "loaded on demand",
            yes_no(has_entry));

        if(!already_loaded) {
            // Probe only. Phase 2 will load it for real and keep it.
            ::FreeLibrary(compiler);
        }
    }

    TW_LOG_INFO("sky_caps: d3dx9_38.dll (the game's own, fallback compiler): {}",
        ::GetModuleHandleA("d3dx9_38.dll") != nullptr ? "loaded" : "not loaded");
}
} // namespace

namespace tw::skybox::caps
{
void report(IDirect3DDevice9* device) noexcept
{
    if(device == nullptr || g_reported) {
        return;
    }
    g_reported = true;

    D3DDEVICE_CREATION_PARAMETERS params {};
    const bool have_params = SUCCEEDED(device->GetCreationParameters(&params));
    if(have_params) {
        TW_LOG_INFO("sky_caps: adapter {}, device type {}, BehaviorFlags=0x{:08X} [{}]",
            static_cast<unsigned long>(params.AdapterOrdinal),
            static_cast<int>(params.DeviceType),
            static_cast<unsigned long>(params.BehaviorFlags),
            behavior_flag_names(params.BehaviorFlags));
    }
    else {
        TW_LOG_WARNING("sky_caps: GetCreationParameters failed");
    }

    // Not part of any state block in D3D9 (it was a render state in D3D8 and became a method here),
    // so if the game ever flips it mid-frame, anything we draw has to save and restore it by hand.
    TW_LOG_INFO("sky_caps: software vertex processing currently {}", device->GetSoftwareVertexProcessing() ? "ON" : "off");

    D3DCAPS9 caps {};
    if(FAILED(device->GetDeviceCaps(&caps))) {
        TW_LOG_ERROR("sky_caps: GetDeviceCaps failed - shader support unknown");
        return;
    }

    TW_LOG_INFO("sky_caps: vertex shader {}.{}, pixel shader {}.{}",
        static_cast<int>(D3DSHADER_VERSION_MAJOR(caps.VertexShaderVersion)),
        static_cast<int>(D3DSHADER_VERSION_MINOR(caps.VertexShaderVersion)),
        static_cast<int>(D3DSHADER_VERSION_MAJOR(caps.PixelShaderVersion)),
        static_cast<int>(D3DSHADER_VERSION_MINOR(caps.PixelShaderVersion)));

    // The number the shader budget in skybox-procedural.md is measured against. The spec floor for
    // ps_3_0 is 512; the heaviest candidate measured 487, so anything at or above the floor is
    // enough for everything planned.
    TW_LOG_INFO("sky_caps: ps_3_0 instruction slots {}, vs_3_0 instruction slots {}",
        static_cast<unsigned long>(caps.MaxPixelShader30InstructionSlots),
        static_cast<unsigned long>(caps.MaxVertexShader30InstructionSlots));

    TW_LOG_INFO("sky_caps: cube maps {}, max texture {}x{}, simultaneous RTs {}, vertex shader constants {}",
        yes_no((caps.TextureCaps & D3DPTEXTURECAPS_CUBEMAP) != 0),
        static_cast<unsigned long>(caps.MaxTextureWidth),
        static_cast<unsigned long>(caps.MaxTextureHeight),
        static_cast<unsigned long>(caps.NumSimultaneousRTs),
        static_cast<unsigned long>(caps.MaxVertexShaderConst));

    TW_LOG_INFO("sky_caps: hardware T&L {} (this is the caps bit CreateD3D branches on to pick MIXED over SOFTWARE)",
        yes_no((caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) != 0));

    TW_LOG_INFO("sky_caps: available texture memory {} MB", static_cast<unsigned long>(device->GetAvailableTextureMem() / (1024 * 1024)));

    if(have_params) {
        report_render_target_formats(device, params);
    }
    report_compiler_availability();
}

void reset() noexcept
{
    g_reported = false;
}
} // namespace tw::skybox::caps
