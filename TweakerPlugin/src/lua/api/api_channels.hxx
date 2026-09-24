#pragma once

// The channel half of the C ABI scripts call: resolving a handle, reading and writing it through its
// family, and Array Table columns.
//
// Everything in src/lua/api/ is `extern "C"`, POD-only, and free of anything that can throw or that
// owns a destructor. Two reasons, both from Docs/Internal/lua-scripting.md:
//
//  - §2.2: these are reached from Lua through the FFI, not as lua_CFunction. A lua_CFunction call
//    is NYI for LuaJIT's trace compiler; an FFI call is compiled into the trace as a direct call.
//    That is the whole performance argument for the scripting layer, so the boundary has to look
//    like C.
//  - §2.3: a Lua error unwinds with longjmp, which skips C++ destructors in every frame it crosses.
//    Keeping this layer destructor-free makes that structurally harmless rather than a rule someone
//    has to remember.
//
// Not exported from the DLL: lua_vm hands the addresses to the VM at bootstrap, **by name**, and the
// prelude ffi.cast()s them. See the entry table in lua_vm.cxx - that is the one list, and a function
// here that is not on it is unreachable from Lua.
namespace tw::lua::api
{
extern "C" {
// Why a resolve did not produce a handle. The distinction between "not yet" and "wrong" is the whole
// point: the first is the normal state during startup and must stay silent, the second is a mistake
// in the script and must be loud.
enum resolve_status : int {
    resolve_ok = 0,
    resolve_engine_pending = 1, // the engine pointer has not been captured yet - retry later
    resolve_no_group = 2,       // group not loaded (can be transient - groups load and unload)
    resolve_no_channel = 3,     // group is there, no channel by that name (usually a typo)
    resolve_wrong_kind = 4,     // the channel exists and is a different family - never transient
    resolve_unusable = 5,       // right family, but its vtable slot is not code. Should not happen
};

// Resolves a channel of an expected kind. Cold path - a name lookup is a linear _stricmp scan over
// the whole group, so callers keep the handle rather than repeating this.
//
// `kind` is tw::engine::kind. `out` receives two ints: [0] a resolve_status, and [1] the kind
// the channel actually turned out to be (only meaningful on resolve_wrong_kind). Two out-ints rather
// than a struct return, for the x86 ABI reasons in Docs/Internal/lua-scripting.md §8.3 - the Lua side
// keeps one reusable cdata buffer, so this costs no allocation per call either.
//
// Returns null unless the status is resolve_ok. Otherwise the handle is a **heap-allocated
// tw::engine::channel_ref**, owned by whoever holds it and released with tw_ref_free - the prelude
// ties that to the Lua handle with ffi.gc, so a handle that is dropped or re-resolved is freed by the
// collector. Opaque to Lua: it is only ever passed back into the functions below.
void* tw_channel_resolve(const char* group, const char* name, int kind, int* out) noexcept;

// Same, addressing the channel by its index in the group instead of by name. Channel names are not
// unique - see engine::channels::find_channel_at - so an index is sometimes the only way to be precise.
void* tw_channel_resolve_at(const char* group, int index, int kind, int* out) noexcept;

// Frees a handle from either resolve. Null-safe. Registered as the handle's ffi.gc finalizer.
void tw_ref_free(void* handle) noexcept;

// Human-readable name of a tw::engine::kind, for error messages.
const char* tw_kind_name(int kind) noexcept;

// Evaluates a numeric channel through its own vtable. Cheap; safe to call every frame.
//
// Every channel entry point below goes through a family function under src/engine/family/, and those
// refuse a handle of any family but their own - one compare. So a handle passed to the wrong entry
// point gives 0 / "" / false, never a call through the wrong slot. That used to be a promise the
// prelude kept; since Ф3 it is enforced where the slot is called.
float tw_channel_get(void* channel) noexcept;

// Writes a numeric channel. The handle must have resolved as numeric; on an Aco_FloatChannel this is
// a plain store, and what the game makes of an unexpected value is entirely up to the game (see
// Docs/Internal/lua-scripting.md §8.1).
void tw_channel_set(void* channel, float value) noexcept;

// Reads a text channel, converting from wide storage when that is the mode it is in. The returned
// pointer is valid until the next call; Lua copies it into a string on the way through the FFI, so
// scripts never see the lifetime.
const char* tw_channel_text(void* channel) noexcept;

// Reads a vector channel into three floats. The handle must have resolved as the vector kind. Zero
// on failure, leaving `out` untouched.
//
// The game keeps its live block palette in vector channels, which is what this exists for: a script
// that colours its own UI by traffic colour can take the player's actual colour settings rather than
// hardcoding a guess at them.
int tw_channel_vector(void* channel, float* out) noexcept;

// Writes a vector channel. The handle must have resolved as the vector kind - that is what keeps
// this off the numeric family's incompatible slot-19 setter (see engine/family/fam_vector.hxx).
//
// Wider effect than tw_channel_set: the engine's own SetVector also writes each component through
// into the numeric channel wired to that port, if there is one.
void tw_channel_set_vector(void* channel, float x, float y, float z) noexcept;

// A matrix channel - Aco_MatrixChannel and its derivatives (engine/family/fam_matrix.hxx). Sixteen
// floats, row-major, D3DXMATRIX layout. Zero on failure, `out` untouched.
int tw_channel_matrix(void* channel, float* out) noexcept;

// Writes all sixteen elements. Subject to the write gate.
void tw_channel_set_matrix(void* channel, const float* in) noexcept;

// Whether the engine evaluated this channel in the current frame of its group: 1 yes, 0 no, -1
// cannot be known (CHIC = 1 - the channel is never memoised, so the field that would say is never
// written). See engine::channels::live() for what this can and cannot mean.
int tw_channel_live(void* channel) noexcept;

// Reads one cell of an Array Table column through its cursor pair. Both handles must have resolved
// as numeric channels; the cursor is saved and restored around the read.
float tw_array_read(void* array_value, void* indexer, float index) noexcept;

// Same for an `Array Vector` column: the column handle must have resolved as the vector kind, the
// cursor as numeric. Writes x/y/z into `out`; zero on failure.
int tw_array_read_vector(void* array_vector, void* indexer, float index, float* out) noexcept;

// Writes one cell of an Array Table column. Zero when the write did not happen - and unlike the
// scalar setters, "the row does not exist" is one of the reasons, because on this path the engine
// would otherwise *create* it and quietly lengthen the game's table (engine journal §2.2.3).
//
// Subject to the same write gate as tw_channel_set.
int tw_array_write(void* array_value, void* indexer, float index, float value) noexcept;
int tw_array_write_vector(void* array_vector, void* indexer, float index, float x, float y, float z) noexcept;

// How many rows that column's table has, or -1 if it cannot be asked. Exposed rather than kept
// private because the bounds check needs it anyway, and without it a script has to discover the
// length of a table the way traffic.lua does - through some unrelated counter channel the game
// happens to keep nearby.
int tw_array_rows(void* column) noexcept;
}
} // namespace tw::lua::api
