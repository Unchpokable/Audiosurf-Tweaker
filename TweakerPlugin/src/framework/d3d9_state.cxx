#include "pch.hxx"

#include "framework/d3d9_state.hxx"

#include "plugin/diagnostics.hxx"

namespace
{
void release_object(IUnknown* object) noexcept
{
    if(object != nullptr) {
        object->Release();
    }
}
} // namespace

namespace tw::framework::d3d9
{
state_scope::state_scope(IDirect3DDevice9* device) noexcept
    : m_device(device)
    , m_count(0)
    , m_blob_used(0)
    , m_complete(true)
{
}

state_scope::~state_scope() noexcept
{
    restore();
}

state_scope::entry* state_scope::push(kind entry_kind) noexcept
{
    if(m_count >= k_max_entries) {
        m_complete = false;
        TW_LOG_ERROR("d3d9_state: scope full at {} entries - state not set because it could not be undone", k_max_entries);
        return nullptr;
    }

    entry* pushed = &m_entries[m_count++];

    pushed->entry_kind = entry_kind;
    pushed->index = 0;
    pushed->token = 0;
    pushed->count = 0;
    pushed->blob = 0;
    pushed->object = nullptr;

    return pushed;
}

void* state_scope::reserve(std::size_t bytes, std::uint16_t& offset) noexcept
{
    // Four-byte alignment is all any payload here needs (DWORDs, floats, matrices), and every size
    // that reaches this is already a multiple of four - the rounding is here so that stays true if
    // one ever is not.
    const std::size_t aligned = (m_blob_used + 3u) & ~static_cast<std::size_t>(3u);

    if(aligned + bytes > k_blob_bytes) {
        m_complete = false;
        TW_LOG_ERROR("d3d9_state: value arena full ({} of {} bytes used, wanted {})", m_blob_used, k_blob_bytes, bytes);
        return nullptr;
    }

    offset = static_cast<std::uint16_t>(aligned);
    m_blob_used = aligned + bytes;

    return m_blob.data() + aligned;
}

void state_scope::apply_object(kind entry_kind, DWORD index, IUnknown* object) noexcept
{
    switch(entry_kind) {
    case kind::texture:
        m_device->SetTexture(index, reinterpret_cast<IDirect3DBaseTexture9*>(object));
        break;
    case kind::vertex_shader:
        m_device->SetVertexShader(reinterpret_cast<IDirect3DVertexShader9*>(object));
        break;
    case kind::pixel_shader:
        m_device->SetPixelShader(reinterpret_cast<IDirect3DPixelShader9*>(object));
        break;
    case kind::vertex_declaration:
        m_device->SetVertexDeclaration(reinterpret_cast<IDirect3DVertexDeclaration9*>(object));
        break;
    case kind::indices:
        m_device->SetIndices(reinterpret_cast<IDirect3DIndexBuffer9*>(object));
        break;
    case kind::render_target:
        // D3D9 refuses to unbind render target zero, so a null there is never put back - and never
        // has to be, because a device that had nothing bound to it could not have been drawn into
        // in the first place.
        if(object != nullptr || index != 0) {
            m_device->SetRenderTarget(index, reinterpret_cast<IDirect3DSurface9*>(object));
        }
        break;
    case kind::depth_stencil:
        m_device->SetDepthStencilSurface(reinterpret_cast<IDirect3DSurface9*>(object));
        break;
    default:
        break;
    }
}

void state_scope::bind_object(kind entry_kind, DWORD index, IUnknown* current, IUnknown* wanted) noexcept
{
    // `current` arrives with a reference this function owns: either it becomes the entry's, or it is
    // dropped here.
    if(current == wanted) {
        release_object(current);
        return;
    }

    entry* pushed = push(entry_kind);
    if(pushed == nullptr) {
        release_object(current);
        return;
    }

    pushed->index = static_cast<std::uint8_t>(index);
    pushed->object = current;

    apply_object(entry_kind, index, wanted);
}

void state_scope::render_state(D3DRENDERSTATETYPE state, DWORD value) noexcept
{
    if(m_device == nullptr) {
        return;
    }

    DWORD previous = 0;
    if(FAILED(m_device->GetRenderState(state, &previous))) {
        m_complete = false;
        return;
    }

    if(previous == value) {
        return;
    }

    entry* pushed = push(kind::render_state);
    if(pushed == nullptr) {
        return;
    }

    pushed->token = static_cast<std::uint16_t>(state);
    pushed->value = previous;

    m_device->SetRenderState(state, value);
}

void state_scope::stage_state(DWORD stage, D3DTEXTURESTAGESTATETYPE state, DWORD value) noexcept
{
    if(m_device == nullptr) {
        return;
    }

    DWORD previous = 0;
    if(FAILED(m_device->GetTextureStageState(stage, state, &previous))) {
        m_complete = false;
        return;
    }

    if(previous == value) {
        return;
    }

    entry* pushed = push(kind::stage_state);
    if(pushed == nullptr) {
        return;
    }

    pushed->index = static_cast<std::uint8_t>(stage);
    pushed->token = static_cast<std::uint16_t>(state);
    pushed->value = previous;

    m_device->SetTextureStageState(stage, state, value);
}

void state_scope::sampler_state(DWORD sampler, D3DSAMPLERSTATETYPE state, DWORD value) noexcept
{
    if(m_device == nullptr) {
        return;
    }

    DWORD previous = 0;
    if(FAILED(m_device->GetSamplerState(sampler, state, &previous))) {
        m_complete = false;
        return;
    }

    if(previous == value) {
        return;
    }

    entry* pushed = push(kind::sampler_state);
    if(pushed == nullptr) {
        return;
    }

    pushed->index = static_cast<std::uint8_t>(sampler);
    pushed->token = static_cast<std::uint16_t>(state);
    pushed->value = previous;

    m_device->SetSamplerState(sampler, state, value);
}

void state_scope::texture(DWORD stage, IDirect3DBaseTexture9* texture) noexcept
{
    if(m_device == nullptr) {
        return;
    }

    IDirect3DBaseTexture9* current = nullptr;
    if(FAILED(m_device->GetTexture(stage, &current))) {
        m_complete = false;
        return;
    }

    bind_object(kind::texture, stage, current, texture);
}

void state_scope::transform(D3DTRANSFORMSTATETYPE which, const D3DMATRIX& matrix) noexcept
{
    if(m_device == nullptr) {
        return;
    }

    std::uint16_t offset = 0;
    void* saved = reserve(sizeof(D3DMATRIX), offset);
    if(saved == nullptr) {
        return;
    }

    if(FAILED(m_device->GetTransform(which, static_cast<D3DMATRIX*>(saved)))) {
        m_complete = false;
        m_blob_used = offset;
        return;
    }

    if(std::memcmp(saved, &matrix, sizeof(D3DMATRIX)) == 0) {
        m_blob_used = offset;
        return;
    }

    entry* pushed = push(kind::transform);
    if(pushed == nullptr) {
        m_blob_used = offset;
        return;
    }

    pushed->token = static_cast<std::uint16_t>(which);
    pushed->blob = offset;

    m_device->SetTransform(which, &matrix);
}

void state_scope::vertex_shader(IDirect3DVertexShader9* shader) noexcept
{
    if(m_device == nullptr) {
        return;
    }

    IDirect3DVertexShader9* current = nullptr;
    if(FAILED(m_device->GetVertexShader(&current))) {
        m_complete = false;
        return;
    }

    bind_object(kind::vertex_shader, 0, current, shader);
}

void state_scope::pixel_shader(IDirect3DPixelShader9* shader) noexcept
{
    if(m_device == nullptr) {
        return;
    }

    IDirect3DPixelShader9* current = nullptr;
    if(FAILED(m_device->GetPixelShader(&current))) {
        m_complete = false;
        return;
    }

    bind_object(kind::pixel_shader, 0, current, shader);
}

void state_scope::vertex_declaration(IDirect3DVertexDeclaration9* declaration) noexcept
{
    if(m_device == nullptr) {
        return;
    }

    IDirect3DVertexDeclaration9* current = nullptr;
    if(FAILED(m_device->GetVertexDeclaration(&current))) {
        m_complete = false;
        return;
    }

    bind_object(kind::vertex_declaration, 0, current, declaration);
}

void state_scope::fvf(DWORD fvf) noexcept
{
    if(m_device == nullptr) {
        return;
    }

    // The cheap test first: an FVF that already matches means the declaration behind it matches too,
    // and neither has to be read or written.
    DWORD current_fvf = 0;
    if(fvf != 0 && SUCCEEDED(m_device->GetFVF(&current_fvf)) && current_fvf == fvf) {
        return;
    }

    IDirect3DVertexDeclaration9* current = nullptr;
    if(FAILED(m_device->GetVertexDeclaration(&current))) {
        m_complete = false;
        return;
    }

    entry* pushed = push(kind::vertex_declaration);
    if(pushed == nullptr) {
        release_object(current);
        return;
    }

    pushed->object = current;

    m_device->SetFVF(fvf);
}

void state_scope::stream_source(UINT stream, IDirect3DVertexBuffer9* buffer, UINT offset, UINT stride) noexcept
{
    if(m_device == nullptr) {
        return;
    }

    IDirect3DVertexBuffer9* current = nullptr;
    UINT current_offset = 0;
    UINT current_stride = 0;

    if(FAILED(m_device->GetStreamSource(stream, &current, &current_offset, &current_stride))) {
        m_complete = false;
        return;
    }

    if(current == buffer && current_offset == offset && current_stride == stride) {
        release_object(current);
        return;
    }

    std::uint16_t blob = 0;
    auto* saved = static_cast<UINT*>(reserve(2 * sizeof(UINT), blob));
    if(saved == nullptr) {
        release_object(current);
        return;
    }

    saved[0] = current_offset;
    saved[1] = current_stride;

    entry* pushed = push(kind::stream_source);
    if(pushed == nullptr) {
        m_blob_used = blob;
        release_object(current);
        return;
    }

    pushed->index = static_cast<std::uint8_t>(stream);
    pushed->blob = blob;
    pushed->object = current;

    m_device->SetStreamSource(stream, buffer, offset, stride);
}

void state_scope::indices(IDirect3DIndexBuffer9* buffer) noexcept
{
    if(m_device == nullptr) {
        return;
    }

    IDirect3DIndexBuffer9* current = nullptr;
    if(FAILED(m_device->GetIndices(&current))) {
        m_complete = false;
        return;
    }

    bind_object(kind::indices, 0, current, buffer);
}

void state_scope::viewport(const D3DVIEWPORT9& viewport) noexcept
{
    if(m_device == nullptr) {
        return;
    }

    std::uint16_t offset = 0;
    void* saved = reserve(sizeof(D3DVIEWPORT9), offset);
    if(saved == nullptr) {
        return;
    }

    if(FAILED(m_device->GetViewport(static_cast<D3DVIEWPORT9*>(saved)))) {
        m_complete = false;
        m_blob_used = offset;
        return;
    }

    if(std::memcmp(saved, &viewport, sizeof(D3DVIEWPORT9)) == 0) {
        m_blob_used = offset;
        return;
    }

    entry* pushed = push(kind::viewport);
    if(pushed == nullptr) {
        m_blob_used = offset;
        return;
    }

    pushed->blob = offset;

    m_device->SetViewport(&viewport);
}

void state_scope::constants(kind entry_kind, UINT first_register, std::span<const float> values) noexcept
{
    if(m_device == nullptr) {
        return;
    }

    const auto registers = static_cast<UINT>(values.size() / 4);
    if(registers == 0) {
        return;
    }

    const std::size_t bytes = static_cast<std::size_t>(registers) * 4 * sizeof(float);

    std::uint16_t offset = 0;
    void* saved = reserve(bytes, offset);
    if(saved == nullptr) {
        return;
    }

    auto* floats = static_cast<float*>(saved);

    const bool pixel = entry_kind == kind::pixel_constants;
    const HRESULT hr = pixel ? m_device->GetPixelShaderConstantF(first_register, floats, registers)
                             : m_device->GetVertexShaderConstantF(first_register, floats, registers);

    if(FAILED(hr)) {
        // Past the device's register count, most likely. Nothing is uploaded either, so the program
        // draws with whatever was there rather than the frame leaving registers nobody restores.
        m_complete = false;
        m_blob_used = offset;
        return;
    }

    if(std::memcmp(floats, values.data(), bytes) == 0) {
        m_blob_used = offset;
        return;
    }

    entry* pushed = push(entry_kind);
    if(pushed == nullptr) {
        m_blob_used = offset;
        return;
    }

    pushed->token = static_cast<std::uint16_t>(first_register);
    pushed->count = static_cast<std::uint16_t>(registers);
    pushed->blob = offset;

    if(pixel) {
        m_device->SetPixelShaderConstantF(first_register, values.data(), registers);
    }
    else {
        m_device->SetVertexShaderConstantF(first_register, values.data(), registers);
    }
}

void state_scope::vertex_constants(UINT first_register, std::span<const float> values) noexcept
{
    constants(kind::vertex_constants, first_register, values);
}

void state_scope::pixel_constants(UINT first_register, std::span<const float> values) noexcept
{
    constants(kind::pixel_constants, first_register, values);
}

void state_scope::render_target(DWORD index, IDirect3DSurface9* surface) noexcept
{
    if(m_device == nullptr) {
        return;
    }

    // D3DERR_NOTFOUND for an index nothing is bound to. That is a value like any other here - it
    // just restores as "leave it alone", see apply_object.
    IDirect3DSurface9* current = nullptr;
    if(FAILED(m_device->GetRenderTarget(index, &current))) {
        current = nullptr;
    }

    bind_object(kind::render_target, index, current, surface);
}

void state_scope::depth_stencil(IDirect3DSurface9* surface) noexcept
{
    if(m_device == nullptr) {
        return;
    }

    // A device created without a depth buffer fails here rather than returning null, and that is not
    // an error - it means there is nothing to put back.
    IDirect3DSurface9* current = nullptr;
    if(FAILED(m_device->GetDepthStencilSurface(&current))) {
        current = nullptr;
    }

    bind_object(kind::depth_stencil, 0, current, surface);
}

void state_scope::software_vertex_processing(BOOL enabled) noexcept
{
    if(m_device == nullptr) {
        return;
    }

    const BOOL current = m_device->GetSoftwareVertexProcessing();
    if(current == enabled) {
        return;
    }

    entry* pushed = push(kind::software_vertex_processing);
    if(pushed == nullptr) {
        return;
    }

    pushed->value = static_cast<DWORD>(current);

    m_device->SetSoftwareVertexProcessing(enabled);
}

void state_scope::restore() noexcept
{
    if(m_device == nullptr) {
        m_count = 0;
        m_blob_used = 0;
        return;
    }

    // Reverse order, so a state set twice inside one scope ends up holding what it held before the
    // first of the two.
    for(std::size_t i = m_count; i > 0; --i) {
        const entry& one = m_entries[i - 1];

        switch(one.entry_kind) {
        case kind::render_state:
            m_device->SetRenderState(static_cast<D3DRENDERSTATETYPE>(one.token), one.value);
            break;

        case kind::stage_state:
            m_device->SetTextureStageState(one.index, static_cast<D3DTEXTURESTAGESTATETYPE>(one.token), one.value);
            break;

        case kind::sampler_state:
            m_device->SetSamplerState(one.index, static_cast<D3DSAMPLERSTATETYPE>(one.token), one.value);
            break;

        case kind::transform:
            m_device->SetTransform(static_cast<D3DTRANSFORMSTATETYPE>(one.token), reinterpret_cast<const D3DMATRIX*>(&m_blob[one.blob]));
            break;

        case kind::viewport:
            m_device->SetViewport(reinterpret_cast<const D3DVIEWPORT9*>(&m_blob[one.blob]));
            break;

        case kind::stream_source: {
            const auto* saved = reinterpret_cast<const UINT*>(&m_blob[one.blob]);
            m_device->SetStreamSource(one.index, reinterpret_cast<IDirect3DVertexBuffer9*>(one.object), saved[0], saved[1]);
            release_object(one.object);
            break;
        }

        case kind::vertex_constants:
            m_device->SetVertexShaderConstantF(one.token, reinterpret_cast<const float*>(&m_blob[one.blob]), one.count);
            break;

        case kind::pixel_constants:
            m_device->SetPixelShaderConstantF(one.token, reinterpret_cast<const float*>(&m_blob[one.blob]), one.count);
            break;

        case kind::software_vertex_processing:
            m_device->SetSoftwareVertexProcessing(static_cast<BOOL>(one.value));
            break;

        default:
            apply_object(one.entry_kind, one.index, one.object);
            release_object(one.object);
            break;
        }
    }

    m_count = 0;
    m_blob_used = 0;
}
} // namespace tw::framework::d3d9
