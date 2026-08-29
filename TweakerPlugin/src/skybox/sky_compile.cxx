#include "pch.hxx"

#include "skybox/sky_compile.hxx"

#include "plugin/diagnostics.hxx"

#include "resource/resource.hxx"

namespace
{
// Signature of D3DCompile, declared here rather than pulled in with D3Dcompiler.h - see the note in
// pch.hxx. Matches the SDK's pD3DCompile typedef; the two parameters this project always passes
// null are typed as void* so no D3D11-era declarations are needed to say so.
using d3d_compile_fn = HRESULT(WINAPI*)(LPCVOID src_data,
    SIZE_T src_size,
    LPCSTR source_name,
    const void* defines,
    ID3DInclude* include,
    LPCSTR entry_point,
    LPCSTR target,
    UINT flags1,
    UINT flags2,
    ID3D10Blob** out_code,
    ID3D10Blob** out_errors);

// From D3Dcompiler.h. Level 3 is what the build-time fxc step passes as /O3, so a shader behaves the
// same whether it arrived compiled or was compiled here.
constexpr UINT k_optimization_level3 = 1 << 15;

constexpr std::string_view k_target = "ps_3_0";
constexpr std::string_view k_entry_point = "main";

// Where an #include is looked for before the filesystem. The bundled sky_common.hlsli is packed as
// text under this prefix, which is what lets a user's shader include it without a copy on disk.
constexpr std::string_view k_packed_prefix = "shaders/";

HMODULE g_compiler = nullptr;
d3d_compile_fn g_compile = nullptr;
std::string g_backend;

// Compiles run on worker threads now, so the one piece of shared mutable state here - which DLL
// answered - has to be resolved exactly once even if two of them start together.
std::once_flag g_load_once;

// Resolves #include for the compiler: the DLL's packed headers first, then next to the shader.
//
// Always hands back a copy, even for a packed header that is already in memory and will outlive the
// compile. One allocation per include is nothing next to a compile, and the alternative is a Close()
// that has to remember which pointers it owns.
struct include_handler : public ID3DInclude {
    std::filesystem::path base;

    HRESULT STDMETHODCALLTYPE Open(
        D3D_INCLUDE_TYPE /*type*/, LPCSTR file_name, LPCVOID /*parent_data*/, LPCVOID* out_data, UINT* out_bytes) override
    {
        if(file_name == nullptr || out_data == nullptr || out_bytes == nullptr) {
            return E_INVALIDARG;
        }

        *out_data = nullptr;
        *out_bytes = 0;

        std::string contents;
        if(!read_packed(file_name, contents) && !read_from_disk(file_name, contents)) {
            TW_LOG_WARNING(
                "sky_compile: #include \"{}\" not found (looked in the DLL's packed headers and in '{}')", file_name, base.string());
            return E_FAIL;
        }

        char* copy = new(std::nothrow) char[contents.size()];
        if(copy == nullptr) {
            return E_OUTOFMEMORY;
        }

        std::memcpy(copy, contents.data(), contents.size());
        *out_data = copy;
        *out_bytes = static_cast<UINT>(contents.size());

        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Close(LPCVOID data) override
    {
        delete[] static_cast<const char*>(data);
        return S_OK;
    }

private:
    static bool read_packed(std::string_view file_name, std::string& out)
    {
        const std::string key = std::string { k_packed_prefix } + std::string { file_name };

        const auto packed = tw::resource::get_resource(tw::resource::type::text, key);
        if(!packed.has_value()) {
            return false;
        }

        out.assign(packed->text());
        return true;
    }

    bool read_from_disk(std::string_view file_name, std::string& out) const
    {
        if(base.empty()) {
            return false;
        }

        std::error_code ec;
        const std::filesystem::path path = base / std::filesystem::path { file_name };
        if(!std::filesystem::is_regular_file(path, ec)) {
            return false;
        }

        std::ifstream file { path, std::ios::binary };
        if(!file.is_open()) {
            return false;
        }

        out.assign(std::istreambuf_iterator<char> { file }, std::istreambuf_iterator<char> {});
        return true;
    }
};

std::string blob_text(ID3D10Blob* blob)
{
    if(blob == nullptr || blob->GetBufferSize() == 0) {
        return {};
    }

    std::string text { static_cast<const char*>(blob->GetBufferPointer()), blob->GetBufferSize() };

    // The compiler NUL-terminates its message inside the buffer, and the trailing newline is only
    // in the way once this is a single line in a log or a tooltip.
    while(!text.empty() && (text.back() == '\0' || text.back() == '\n' || text.back() == '\r')) {
        text.pop_back();
    }

    return text;
}

void load_compiler_once()
{
    for(const char* name : { "d3dcompiler_47.dll", "d3dcompiler_43.dll" }) {
        HMODULE module = ::GetModuleHandleA(name);
        if(module == nullptr) {
            module = ::LoadLibraryA(name);
        }

        if(module == nullptr) {
            continue;
        }

        const auto entry = reinterpret_cast<d3d_compile_fn>(::GetProcAddress(module, "D3DCompile"));
        if(entry == nullptr) {
            continue;
        }

        g_compiler = module;
        g_compile = entry;
        g_backend = name;

        TW_LOG_INFO("sky_compile: using {}", g_backend);
        return;
    }

    TW_LOG_WARNING("sky_compile: no d3dcompiler found - .hlsl files on disk cannot be compiled, built-in programs are unaffected");
}
} // namespace

namespace tw::skybox::compile
{
bool available() noexcept
{
    std::call_once(g_load_once, &load_compiler_once);
    return g_compile != nullptr;
}

std::string_view backend_name() noexcept
{
    return g_backend;
}

result pixel_shader(std::string_view source, const std::filesystem::path& source_path)
{
    if(!available()) {
        return result { {}, "no d3dcompiler_47.dll on this machine - only the built-in skies can run" };
    }

    if(source.empty()) {
        return result { {}, "the file is empty" };
    }

    include_handler includes;
    includes.base = source_path.parent_path();

    const std::string name = source_path.filename().string();

    ID3D10Blob* code = nullptr;
    ID3D10Blob* errors = nullptr;

    const HRESULT hr = g_compile(source.data(),
        source.size(),
        name.c_str(),
        nullptr,
        &includes,
        k_entry_point.data(),
        k_target.data(),
        k_optimization_level3,
        0,
        &code,
        &errors);

    result out;
    out.diagnostics = blob_text(errors);

    if(errors != nullptr) {
        errors->Release();
    }

    if(FAILED(hr) || code == nullptr) {
        if(out.diagnostics.empty()) {
            out.diagnostics = std::format("D3DCompile failed, hr=0x{:08X}", static_cast<unsigned long>(hr));
        }

        if(code != nullptr) {
            code->Release();
        }

        return out;
    }

    const auto* bytes = static_cast<const std::byte*>(code->GetBufferPointer());
    out.bytecode.assign(bytes, bytes + code->GetBufferSize());
    code->Release();

    return out;
}

result pixel_shader_file(const std::filesystem::path& path)
{
    std::ifstream file { path, std::ios::binary };
    if(!file.is_open()) {
        return result { {}, "could not open " + path.string() };
    }

    std::string source { std::istreambuf_iterator<char> { file }, std::istreambuf_iterator<char> {} };

    result out = pixel_shader(source, path);
    out.source = std::move(source);

    return out;
}

std::future<result> pixel_shader_file_async(std::filesystem::path path)
{
    // std::launch::async, not the default: the deferred policy would run the compile on whichever
    // thread first asks for the result, which is the render thread, which is the whole thing being
    // avoided here.
    return std::async(std::launch::async, [path = std::move(path)] {
        return pixel_shader_file(path);
    });
}
} // namespace tw::skybox::compile
