#include "pch.hxx"

#include "lua/api/api_channels.hxx"

#include "engine/channel_kind.hxx"
#include "engine/channel_ref.hxx"
#include "engine/engine_state.hxx"
#include "engine/family/fam_matrix.hxx"
#include "engine/family/fam_number.hxx"
#include "engine/family/fam_table.hxx"
#include "engine/family/fam_text.hxx"
#include "engine/family/fam_vector.hxx"

#include "lua/lua_diag.hxx"

namespace
{
// Whether the game has finished coming up far enough to be written to.
//
// **This used to be a local heuristic and is now one question asked of engine::state.** The old
// version watched the engine's group count and opened a latch once it had not moved for a second.
// That was the right intuition with the wrong sensor: it caught the end of loading by its side
// effect, knew nothing about what it was watching, and could not say anything about the states after
// the game was up - so it could not be used to hold a script's callbacks back, only its writes.
//
// The signal, the reasoning behind it, and the failure mode it protects against now live in
// src/engine/engine_state.hxx, in one place, for every consumer.
[[nodiscard]] bool writes_allowed() noexcept
{
    return tw::engine::state::ready();
}

// Said once per shape, not per attempt, and not into the notefeed: callbacks do not run before the
// game is up, so the only code that can get here is a script's own body at load time, or an on_frame
// registered with before_ready. Either way the author wants to know and the player does not. The
// write carries no owner, so this is filed under the layer.
void report_write_refused() noexcept
{
    tw::lua::diag::report(-1, tw::lua::diag::level::warn, "write", "a script tried to write to the graph before the game finished loading - refused");
}

// A resolve result handed to Lua: a heap-allocated channel_ref, owned by the Lua handle and freed by
// its ffi.gc finalizer (tw_ref_free). Heap rather than a pool because its life is the handle's, and
// the handle's is the collector's business - a script that resolves the same channel on every
// re-resolve of the graph produces garbage, not a leak.
void* hand_over(const tw::engine::channel_ref& ref) noexcept
{
    return new(std::nothrow) tw::engine::channel_ref(ref);
}

void report(int* out, tw::engine::resolve_status status, tw::engine::kind actual) noexcept
{
    if(out != nullptr) {
        out[0] = static_cast<int>(status);
        out[1] = static_cast<int>(actual);
    }
}

// Every channel entry point below takes what hand_over() returned. Null-tolerant, and nothing more:
// the family check that decides whether a slot may be called lives in the family functions, which
// refuse a ref of any family but their own.
const tw::engine::channel_ref& as_ref(const void* handle) noexcept
{
    static const tw::engine::channel_ref k_empty {};
    return handle != nullptr ? *static_cast<const tw::engine::channel_ref*>(handle) : k_empty;
}
} // namespace

namespace tw::lua::api
{
extern "C" {
void* tw_channel_resolve(const char* group_name, const char* channel_name, int wanted_kind, int* out) noexcept
{
    tw::engine::channel_ref ref {};
    tw::engine::kind actual = tw::engine::kind::unknown;
    const tw::engine::resolve_status status
        = tw::engine::channels::resolve(group_name, channel_name, static_cast<tw::engine::kind>(wanted_kind), ref, &actual);

    if(status != tw::engine::resolve_status::ok) {
        report(out, status, actual);
        return nullptr;
    }

    void* handle = hand_over(ref);
    report(out, handle != nullptr ? status : tw::engine::resolve_status::unusable, actual);
    return handle;
}

void* tw_channel_resolve_at(const char* group_name, int index, int wanted_kind, int* out) noexcept
{
    tw::engine::channel_ref ref {};
    tw::engine::kind actual = tw::engine::kind::unknown;
    const tw::engine::resolve_status status
        = tw::engine::channels::resolve_at(group_name, index, static_cast<tw::engine::kind>(wanted_kind), ref, &actual);

    if(status != tw::engine::resolve_status::ok) {
        report(out, status, actual);
        return nullptr;
    }

    void* handle = hand_over(ref);
    report(out, handle != nullptr ? status : tw::engine::resolve_status::unusable, actual);
    return handle;
}

void tw_ref_free(void* handle) noexcept
{
    delete static_cast<tw::engine::channel_ref*>(handle);
}

const char* tw_kind_name(int kind) noexcept
{
    return tw::engine::channel_kind::name(static_cast<tw::engine::kind>(kind));
}

float tw_channel_get(void* channel) noexcept
{
    return tw::engine::fam_number::get(as_ref(channel));
}

void tw_channel_set(void* channel, float value) noexcept
{
    if(!writes_allowed()) {
        report_write_refused();
        return;
    }

    tw::engine::fam_number::set(as_ref(channel), value);
}

void tw_channel_set_vector(void* channel, float x, float y, float z) noexcept
{
    if(!writes_allowed()) {
        report_write_refused();
        return;
    }

    tw::engine::fam_vector::set(as_ref(channel), x, y, z);
}

const char* tw_channel_text(void* channel) noexcept
{
    return tw::engine::fam_text::get(as_ref(channel));
}

int tw_channel_vector(void* channel, float* out) noexcept
{
    return tw::engine::fam_vector::get(as_ref(channel), out) ? 1 : 0;
}

int tw_channel_matrix(void* channel, float* out) noexcept
{
    return tw::engine::fam_matrix::get(as_ref(channel), out) ? 1 : 0;
}

void tw_channel_set_matrix(void* channel, const float* in) noexcept
{
    if(!writes_allowed()) {
        report_write_refused();
        return;
    }

    tw::engine::fam_matrix::set(as_ref(channel), in);
}

int tw_channel_live(void* channel) noexcept
{
    return static_cast<int>(tw::engine::channels::live(as_ref(channel)));
}

float tw_array_read(void* array_value, void* indexer, float index) noexcept
{
    return tw::engine::fam_table::read(as_ref(array_value), as_ref(indexer), index);
}

int tw_array_read_vector(void* array_vector, void* indexer, float index, float* out) noexcept
{
    return tw::engine::fam_table::read_vector(as_ref(array_vector), as_ref(indexer), index, out) ? 1 : 0;
}

int tw_array_write(void* array_value, void* indexer, float index, float value) noexcept
{
    if(!writes_allowed()) {
        report_write_refused();
        return 0;
    }

    return tw::engine::fam_table::write(as_ref(array_value), as_ref(indexer), index, value) ? 1 : 0;
}

int tw_array_write_vector(void* array_vector, void* indexer, float index, float x, float y, float z) noexcept
{
    if(!writes_allowed()) {
        report_write_refused();
        return 0;
    }

    return tw::engine::fam_table::write_vector(as_ref(array_vector), as_ref(indexer), index, x, y, z) ? 1 : 0;
}

int tw_array_rows(void* column) noexcept
{
    return tw::engine::fam_table::row_count(as_ref(column));
}
}
} // namespace tw::lua::api
