#pragma once

// Scoped save/restore of exactly the device state a hook touched.
//
// Anything that draws inside somebody else's frame owes that frame its state back. The obvious way
// to pay is CreateStateBlock(D3DSBT_ALL) plus Capture() on the way in and Apply() on the way out,
// and that is what the skybox did first - it is three lines and it cannot forget anything. What it
// costs is the whole device: D3DSBT_ALL covers every render state, all eight texture stages, all
// sixteen samplers, 256 world matrices, the lights, and the entire constant file of both shader
// stages (256 vertex registers plus 224 pixel ones - 7.5KB of floats alone). Capture reads all of
// it and Apply pushes all of it back down through the runtime and the driver, twice per
// interception, whether or not a single one of those values differs from what we are about to set.
//
// A sky drawn once a frame can afford that. A hook on a texture or a mesh that the game draws two
// hundred times a frame cannot, and this file exists so that such a hook does not have to invent its
// own save/restore - which is the other way this goes wrong, because a hand-rolled one drifts: the
// list of what gets set and the list of what gets restored are written in two different places and
// only one of them gets updated.
//
// So the two lists are one list here. Every mutation goes through the scope, which reads the old
// value first, writes the new one, and remembers the pair. Whatever a call site changes, it restores
// - by construction rather than by review. Cost is proportional to what the hook actually touches:
// forty-odd Get/Set pairs for the sky instead of the entire device.
//
// Three further properties, none of them accidental:
//
//  - **Nothing is set that cannot be undone.** The saved value is recorded before the write, so an
//    overflowing or unreadable state is skipped rather than written and lost. Worst case the pass
//    draws wrong; the game never inherits a state nobody remembers changing.
//
//  - **Redundant writes are dropped.** The old value has just been read, so comparing it against the
//    new one is free. A state that already holds what we want costs nothing on the way in and
//    nothing on the way out - which for a hook whose state mostly matches the game's is most of the
//    list.
//
//  - **It is not a device object.** No CreateStateBlock, no COM allocation, no lifetime tied to the
//    device, and therefore nothing to release before Reset and nothing to rebuild after it. A scope
//    is a stack object; its whole life is one interception.
//
// It also covers the three things no state block can: the render target, the depth-stencil surface,
// and SetSoftwareVertexProcessing.
//
// **Composition is by nesting, not by sharing.** Whoever draws opens their own scope; a pass called
// from inside another pass restores to what its caller had set, and the caller then restores to what
// the game had. That is why nothing here needs to be passed down through a call chain, and why a
// nested pass cannot be broken by a change to the pass above it. The overlapping states are read and
// written twice, which is a handful of calls, and it buys locality that a shared scope does not have.
//
// **Threading**: none. A scope belongs to whichever thread is inside the hook - the render thread in
// practice - and is neither copyable nor movable. Two scopes on two threads against one device would
// be a data race in D3D9 itself long before they were one here.

namespace tw::framework::d3d9
{
class state_scope
{
public:
    explicit state_scope(IDirect3DDevice9* device) noexcept;

    // Restores. Doing it here rather than only in an explicit call is what makes an early return out
    // of a half-applied pass safe.
    ~state_scope() noexcept;

    state_scope(const state_scope&) = delete;
    state_scope& operator=(const state_scope&) = delete;
    state_scope(state_scope&&) = delete;
    state_scope& operator=(state_scope&&) = delete;

    // Each of these reads the current value, records it, and writes the new one - skipping the write
    // entirely when the two already agree. Call them instead of the device method of the same name;
    // a direct device->Set* inside a scope is a state that will not be put back.
    void render_state(D3DRENDERSTATETYPE state, DWORD value) noexcept;
    void stage_state(DWORD stage, D3DTEXTURESTAGESTATETYPE state, DWORD value) noexcept;
    void sampler_state(DWORD sampler, D3DSAMPLERSTATETYPE state, DWORD value) noexcept;
    void texture(DWORD stage, IDirect3DBaseTexture9* texture) noexcept;
    void transform(D3DTRANSFORMSTATETYPE which, const D3DMATRIX& matrix) noexcept;
    void vertex_shader(IDirect3DVertexShader9* shader) noexcept;
    void pixel_shader(IDirect3DPixelShader9* shader) noexcept;
    void stream_source(UINT stream, IDirect3DVertexBuffer9* buffer, UINT offset, UINT stride) noexcept;
    void indices(IDirect3DIndexBuffer9* buffer) noexcept;
    void viewport(const D3DVIEWPORT9& viewport) noexcept;

    // Saves the vertex *declaration* and not the FVF, then calls SetFVF. Those are the same state
    // seen from two sides: SetFVF builds an implicit declaration and installs it, so a device the
    // game left with a real declaration would be handed back an FVF of zero if the FVF were what got
    // saved. Putting the declaration back restores either case, and GetFVF still answers correctly
    // afterwards when the declaration came from one (verified against a live device in
    // harness/state).
    void fvf(DWORD fvf) noexcept;
    void vertex_declaration(IDirect3DVertexDeclaration9* declaration) noexcept;

    // Four floats per register, starting at `first_register`. Only the registers covered by `values`
    // are read and written - the gaps between the runs a shader declares belong to its own literals,
    // and a scope that saved the whole file would be as expensive as the state block this replaces.
    void vertex_constants(UINT first_register, std::span<const float> values) noexcept;
    void pixel_constants(UINT first_register, std::span<const float> values) noexcept;

    // The three a state block never covered. `render_target`/`depth_stencil` treat "nothing bound"
    // as a value like any other, so a pass that binds an offscreen surface gets the back buffer put
    // back even on a device without a depth buffer.
    void render_target(DWORD index, IDirect3DSurface9* surface) noexcept;
    void depth_stencil(IDirect3DSurface9* surface) noexcept;
    void software_vertex_processing(BOOL enabled) noexcept;

    // Puts everything back, in reverse order, and releases the references held on the way. Idempotent
    // - the destructor calls it, and calling it early simply leaves the destructor nothing to do.
    void restore() noexcept;

    // False when something could not be saved and was therefore not set: a Get that failed, or more
    // state than the scope holds. The device is still consistent - that is the point of the ordering
    // - but the pass drew with state it did not ask for, so a caller that can skip its draw should.
    [[nodiscard]] bool complete() const noexcept
    {
        return m_complete;
    }

private:
    // Sized for the worst case this plugin has: a shaded sky binding sixteen samplers with their
    // filtering and address modes, plus the render states, the geometry bindings and the constant
    // runs. Costs 3KB of stack for the table and 8KB for the values; a scope is short-lived and lives
    // on the render thread's stack, which has both to spare.
    static constexpr std::size_t k_max_entries = 256;
    static constexpr std::size_t k_blob_bytes = 8192;

    enum class kind : std::uint8_t {
        render_state,
        stage_state,
        sampler_state,
        texture,
        transform,
        vertex_shader,
        pixel_shader,
        vertex_declaration,
        stream_source,
        indices,
        viewport,
        vertex_constants,
        pixel_constants,
        render_target,
        depth_stencil,
        software_vertex_processing,
    };

    // Twelve bytes, and deliberately not a variant: the payload is either one DWORD, one COM pointer
    // whose reference this entry owns, or an offset into the blob below.
    struct entry {
        kind entry_kind;
        std::uint8_t index;  // texture stage, sampler, stream or render target index
        std::uint16_t token; // D3DRS_*/D3DTSS_*/D3DSAMP_*/D3DTS_*, or the first shader register
        std::uint16_t count; // shader constant registers
        std::uint16_t blob;  // byte offset into m_blob for matrices, viewports, constants, stream tuples

        union {
            DWORD value;
            IUnknown* object;
        };
    };

    // Returns nullptr - having marked the scope incomplete - when the table is full. A caller that
    // gets nullptr must not perform its write.
    entry* push(kind entry_kind) noexcept;

    // Reserves `bytes` of the value arena. Same contract as push(): nullptr means do not write.
    void* reserve(std::size_t bytes, std::uint16_t& offset) noexcept;

    // The one place that knows which Set* belongs to which kind of binding, so the write on the way
    // in and the write on the way out cannot disagree about it.
    void apply_object(kind entry_kind, DWORD index, IUnknown* object) noexcept;

    // Takes over the reference `current` arrives with: it either becomes the entry's or is released
    // here.
    void bind_object(kind entry_kind, DWORD index, IUnknown* current, IUnknown* wanted) noexcept;
    void constants(kind entry_kind, UINT first_register, std::span<const float> values) noexcept;

    IDirect3DDevice9* m_device;
    std::size_t m_count;
    std::size_t m_blob_used;
    bool m_complete;

    // Uninitialised on purpose: only the first m_count entries and the first m_blob_used bytes are
    // ever read, and zeroing 11KB per interception is exactly the kind of cost this file exists to
    // avoid.
    std::array<entry, k_max_entries> m_entries;
    alignas(16) std::array<std::byte, k_blob_bytes> m_blob;
};
} // namespace tw::framework::d3d9
