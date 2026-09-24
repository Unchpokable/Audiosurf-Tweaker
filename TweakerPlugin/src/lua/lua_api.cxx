#include "pch.hxx"

#include "lua/lua_api.hxx"

#include "engine/channel_kind.hxx"
#include "engine/channel_ref.hxx"
#include "engine/engine_frame.hxx"
#include "engine/engine_groups.hxx"
#include "engine/engine_state.hxx"
#include "engine/family/fam_matrix.hxx"
#include "engine/family/fam_number.hxx"
#include "engine/family/fam_table.hxx"
#include "engine/family/fam_text.hxx"
#include "engine/family/fam_vector.hxx"

#include "framework/channel_shim.hxx"

#include "libtweeny/tweeny.hxx"

#include "lua/lua_host.hxx"

#include "plugin/diagnostics.hxx"

#include "ui/fonts.hxx"
#include "ui/image/svg.hxx"
#include "ui/plugins/interactive/menu.hxx"
#include "ui/plugins/static/notefeed.hxx"
#include "ui/plugins/static/pins.hxx"
#include "ui/plugins/static/watermark.hxx"
#include "ui/theme.hxx"
#include "ui/widgets/detail/draw.hxx"

#include <imgui.h>
#include <imgui_internal.h>

namespace
{
// One per tw.on_call / tw.mute subscription. Only the id crosses back into Lua: C never holds a Lua
// value, so there is nothing here for the collector to interact with and nothing to unregister in
// the right order.
struct subscription {
    int id;
    // Which script asked for this. Every subscription is attributable, because disabling a script
    // has to take exactly its own hooks back out and leave everyone else's in place - see
    // tw_unsubscribe_owner.
    int owner;

    // Null while detached, which is a normal state and not an error: a subscription is accepted the
    // moment a script asks for it, and attached whenever its group turns up. A script that hooks a
    // channel in the `Renderer` pool detaches at the end of every run and re-attaches at the start
    // of the next one without noticing, because its id never changes.
    A3d_Channel* channel;
    A3d_ChannelGroup* group;
    tw::engine::groups::generation gen;

    // How the script asked, kept so the subscription can be resolved again after the group comes
    // back. `index` is -1 when it was addressed by name.
    std::string group_name;
    std::string channel_name;
    int index;

    bool after;
    // A mute answers "do not run the original" from C and never enters the VM. Kept in the same
    // table as the Lua subscriptions so one clear releases both, and so a script cannot mute and
    // observe the same channel through two vtable copies.
    bool mute;
    bool enabled;
};

std::vector<subscription*> g_subscriptions;
int g_next_subscription_id = 0;

// Script corner mask -> ImDrawFlags. Not a cast: ImGui's "no corners" is 1<<8 and its zero means
// "all corners", so a script that passed 0 meaning none would get all of them, silently. See the
// note on hud_corner in lua_api.hxx.
ImDrawFlags to_draw_flags(int corners) noexcept
{
    if((corners & tw::lua::api::hud_corner_all) == 0) {
        return ImDrawFlags_RoundCornersNone;
    }

    ImDrawFlags flags = 0;
    if((corners & tw::lua::api::hud_corner_top_left) != 0) {
        flags |= ImDrawFlags_RoundCornersTopLeft;
    }
    if((corners & tw::lua::api::hud_corner_top_right) != 0) {
        flags |= ImDrawFlags_RoundCornersTopRight;
    }
    if((corners & tw::lua::api::hud_corner_bottom_left) != 0) {
        flags |= ImDrawFlags_RoundCornersBottomLeft;
    }
    if((corners & tw::lua::api::hud_corner_bottom_right) != 0) {
        flags |= ImDrawFlags_RoundCornersBottomRight;
    }

    return flags;
}

// The easing curves offered to scripts, by index. Appended to, never reordered - an index is part
// of the script-facing ABI, exactly like a font index.
//
// A subset of tweeny's set rather than all 33: these are the ones an overlay animation actually
// reaches for, and a name a script has to guess at is worse than one that is not there.
struct ease_curve {
    const char* name;
    float (*run)(float t);
};

const ease_curve g_ease_curves[] = {
    { "linear", [](float t) { return tweeny::easing::linear.run(t, 0.f, 1.f); } },
    { "quadIn", [](float t) { return tweeny::easing::quadraticIn.run(t, 0.f, 1.f); } },
    { "quadOut", [](float t) { return tweeny::easing::quadraticOut.run(t, 0.f, 1.f); } },
    { "quadInOut", [](float t) { return tweeny::easing::quadraticInOut.run(t, 0.f, 1.f); } },
    { "cubicIn", [](float t) { return tweeny::easing::cubicIn.run(t, 0.f, 1.f); } },
    { "cubicOut", [](float t) { return tweeny::easing::cubicOut.run(t, 0.f, 1.f); } },
    { "cubicInOut", [](float t) { return tweeny::easing::cubicInOut.run(t, 0.f, 1.f); } },
    { "sineIn", [](float t) { return tweeny::easing::sinusoidalIn.run(t, 0.f, 1.f); } },
    { "sineOut", [](float t) { return tweeny::easing::sinusoidalOut.run(t, 0.f, 1.f); } },
    { "sineInOut", [](float t) { return tweeny::easing::sinusoidalInOut.run(t, 0.f, 1.f); } },
    { "expoIn", [](float t) { return tweeny::easing::exponentialIn.run(t, 0.f, 1.f); } },
    { "expoOut", [](float t) { return tweeny::easing::exponentialOut.run(t, 0.f, 1.f); } },
    { "expoInOut", [](float t) { return tweeny::easing::exponentialInOut.run(t, 0.f, 1.f); } },
    { "backIn", [](float t) { return tweeny::easing::backIn.run(t, 0.f, 1.f); } },
    { "backOut", [](float t) { return tweeny::easing::backOut.run(t, 0.f, 1.f); } },
    { "backInOut", [](float t) { return tweeny::easing::backInOut.run(t, 0.f, 1.f); } },
    { "elasticOut", [](float t) { return tweeny::easing::elasticOut.run(t, 0.f, 1.f); } },
    { "bounceOut", [](float t) { return tweeny::easing::bounceOut.run(t, 0.f, 1.f); } },
};

subscription* find_subscription(int id) noexcept
{
    for(subscription* record : g_subscriptions) {
        if(record->id == id) {
            return record;
        }
    }

    return nullptr;
}

// The bridge from the engine's call stack back into Lua. Runs on the engine thread, inside whatever
// the game was doing - not inside an ImGui frame, so unlike on_frame there is no draw state to
// protect here; lua_host::dispatch_call does the pcall and the error containment.
//
// Returns whether the engine's own handler should still run. Only the "before" phase can say no.
bool dispatch_channel_call(A3d_Channel* /*channel*/, void* user, tw::framework::channel_shim::phase when)
{
    const auto* record = static_cast<const subscription*>(user);

    if(record->mute) [[unlikely]] {
        // The whole reason a mute is not just an on_call handler that returns false: this is the
        // entire cost of one, on a node the game calls every frame for the rest of the session.
        return !record->enabled;
    }

    const bool wanted = record->after ? (when == tw::framework::channel_shim::phase::after)
                                      : (when == tw::framework::channel_shim::phase::before);
    if(!wanted) {
        return true;
    }

    // A script only gets to cancel from a "before" handler. Letting an "after" one return false
    // would be meaningless - the call it would be declining has already happened.
    const bool proceed = tw::lua::host::dispatch_call(record->id);

    return record->after || proceed;
}

// Said once, with the name the script used, and only for the answer that will never change.
//
// **The distinction this rests on is the whole reason waiting stopped being polling.** "The group is
// not loaded" is transient and says nothing - the menu does not have a run's groups, and never will
// until there is a run. "The group is loaded and has no such channel" is final: a group's channel
// list is fixed when it loads, so no amount of waiting will produce one. That is a typo, and a typo
// that stays silent forever is worse than a noisy one.
void report_missing_target(const subscription& record, tw::lua::api::resolve_status status) noexcept
{
    const char* const what = record.mute ? "mute" : "on_call";

    if(status == tw::lua::api::resolve_unusable) {
        TW_LOG_WARNING("lua_api: {} target {} could not be hooked", what, record.group_name);
        tw::ui::plugins::statics::notefeed::push(std::format("Lua: {} target in {} could not be hooked", what, record.group_name));
        return;
    }

    if(record.index >= 0) {
        TW_LOG_WARNING("lua_api: {} target {}#{} does not exist in that group", what, record.group_name, record.index);
        tw::ui::plugins::statics::notefeed::push(
            std::format("Lua: {}.#{}: no such channel in group", record.group_name, record.index));
        return;
    }

    TW_LOG_WARNING("lua_api: {} target {}.{} does not exist in that group", what, record.group_name, record.channel_name);
    tw::ui::plugins::statics::notefeed::push(std::format("Lua: {}.{}: no such channel in group", record.group_name, record.channel_name));
}

// Tries to bind one record to a live channel. No kind check on purpose: CallChannel is slot 1 of the
// *base* vtable, so every channel of every family has it - this is the one thing that does not care
// what the channel is.
//
// Returns the status a script would want to hear about, and leaves the record detached on anything
// but `resolve_ok`. It is called twice for most subscriptions: once when the script registers, and
// again from the group registry when the group it was waiting for turns up.
tw::lua::api::resolve_status attach_subscription(subscription* record) noexcept
{
    if(record->channel != nullptr) {
        return tw::lua::api::resolve_ok;
    }

    if(!tw::engine::channels::available()) {
        return tw::lua::api::resolve_engine_pending;
    }

    A3d_ChannelGroup* const group = tw::engine::channels::find_group(record->group_name.c_str());
    if(group == nullptr) {
        return tw::lua::api::resolve_no_group;
    }

    A3d_Channel* const channel = record->index < 0 ? tw::engine::channels::find_channel(group, record->channel_name.c_str())
                                                   : tw::engine::channels::find_channel_at(group, record->index);
    if(channel == nullptr) {
        return tw::lua::api::resolve_no_channel;
    }

    // The record doubles as the subscriber key: it is unique per subscription, which is exactly what
    // the shim needs to tell two scripts watching the same channel apart. The group goes with it so
    // that the shim can be torn off the whole group at once when it is destroyed.
    if(!tw::framework::channel_shim::subscribe(channel, group, &dispatch_channel_call, record)) {
        return tw::lua::api::resolve_unusable;
    }

    record->channel = channel;
    record->group = group;
    record->gen = tw::engine::groups::generation_of(group);

    return tw::lua::api::resolve_ok;
}

// Registers a subscription, shared by the by-name and by-index forms of on_call and mute.
//
// **A group that is not loaded is not a failure any more.** It used to be: the call returned -1 and
// Lua re-tried it every 60 dispatches, forever, which cost a full group scan per attempt and never
// stopped for a name that would never resolve. Now the record is kept and the group registry
// attaches it the moment that group appears, so waiting costs nothing at all.
//
// -1 still means "this will not happen": either the group is loaded and has no such channel - a
// final answer, because a group's channel list does not change once it is loaded - or the channel
// could not be hooked. Both are reported here rather than left for Lua to count misses.
int resolve_and_subscribe(
    const char* group_name, const char* channel_name, int index, int owner, int after, bool mute, int* out) noexcept
{
    const auto set = [out](tw::lua::api::resolve_status status) {
        if(out != nullptr) {
            out[0] = status;
            out[1] = static_cast<int>(tw::engine::kind::unknown);
        }
    };

    auto* record = new(std::nothrow) subscription {};
    if(record == nullptr) {
        set(tw::lua::api::resolve_unusable);
        return -1;
    }

    record->id = g_next_subscription_id;
    record->owner = owner;
    record->channel = nullptr;
    record->group = nullptr;
    record->gen = tw::engine::groups::no_generation;
    record->group_name = group_name != nullptr ? group_name : "";
    record->channel_name = channel_name != nullptr ? channel_name : "";
    record->index = channel_name != nullptr ? -1 : index;
    record->after = after != 0;
    record->mute = mute;
    record->enabled = true;

    const tw::lua::api::resolve_status status = attach_subscription(record);

    if(status == tw::lua::api::resolve_no_channel || status == tw::lua::api::resolve_unusable) {
        report_missing_target(*record, status);
        delete record;
        set(status);
        return -1;
    }

    g_subscriptions.push_back(record);
    ++g_next_subscription_id;

    set(status);

    return record->id;
}

// A group is being destroyed. The shim copies are already off its channels by the time this runs -
// engine_groups does that first, before any listener - so all that is left is to forget the
// pointers, which puts the subscription back into the waiting state it started in.
void on_group_unloading(A3d_ChannelGroup* group, tw::engine::groups::generation /*gen*/) noexcept
{
    for(subscription* record : g_subscriptions) {
        if(record->group == group) {
            record->channel = nullptr;
            record->group = nullptr;
            record->gen = tw::engine::groups::no_generation;
        }
    }
}

// The roster moved. Cold path by construction: it runs only on the frames where a group actually
// appeared or disappeared, which during play is the start and the end of a run.
void on_groups_changed() noexcept
{
    for(subscription* record : g_subscriptions) {
        if(record->channel != nullptr) {
            continue;
        }

        if(attach_subscription(record) == tw::lua::api::resolve_no_channel) {
            report_missing_target(*record, tw::lua::api::resolve_no_channel);
        }
    }
}

// The two halves of tw.frame (see tw_frame): a count of draw dispatches, used only while the engine
// spine has never ticked, and the monotonic value actually handed out.
//
// Both are touched from the engine/render thread only - tick() runs from lua_host::draw_frame, and
// tw_frame() from the dispatchers, all of which are that one thread (engine journal §4.1).
std::uint32_t g_draw_frames = 0;
std::uint32_t g_frame = 0;

bool g_write_gate_warned = false;

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

// Said once, not per attempt: a script that writes every frame would otherwise paper the screen over
// while the game is still loading.
void report_write_refused() noexcept
{
    if(g_write_gate_warned) {
        return;
    }

    g_write_gate_warned = true;
    TW_LOG_WARNING("lua_api: a script tried to write to the graph before the game finished loading - refused");
    tw::ui::plugins::statics::notefeed::push("Lua: write refused - the game is still loading");
}

// Whether it is safe to touch ImGui at all right now, and specifically to add to a draw list.
//
// Scripts draw from on_frame, which the overlay calls from inside its own ImGui frame - fine. But
// nothing stops a script calling tw.hud.text from an on_call handler instead, and those run on the
// engine's call stack, in the middle of the game's own logic, with no frame open and possibly before
// the overlay has an ImGui context at all. Adding to a draw list there is at best output that is
// discarded at the next NewFrame and at worst a null dereference inside the game.
//
// So the drawing entry points check, and quietly do nothing outside a frame. Quietly, because the
// alternative - a Lua error - would fire from inside the game's graph walk and trip the one-strike
// latch on a script whose only mistake was drawing from the wrong callback.
[[nodiscard]] bool inside_frame() noexcept
{
    const ImGuiContext* context = ImGui::GetCurrentContext();
    return context != nullptr && context->WithinFrameScope;
}

// Weaker check for the read-only geometry calls: they do not mutate anything, so they only need a
// context to exist. A script asking for the viewport size from a channel hook gets a real answer.
[[nodiscard]] bool imgui_ready() noexcept
{
    return ImGui::GetCurrentContext() != nullptr;
}

// The overlay's palette, by name. Pointers rather than copies because the theme is live - it is
// re-applied by from_config() and edited by the Settings colour pickers, and a script asking for
// "surface" wants what the overlay is using right now.
//
// The whole set is exposed rather than a curated subset: the point is that a script's chrome matches
// the overlay's, and deciding for the author which halves of the palette they are allowed to match
// would just push them back to inventing their own colours.
struct theme_entry {
    const char* name;
    const ImVec4* color;
};

const theme_entry g_theme[] = {
    { "accent_primary", &tw::ui::theme::accent_primary },
    { "accent_secondary", &tw::ui::theme::accent_secondary },
    { "accent_text", &tw::ui::theme::accent_text },
    { "accent_selection", &tw::ui::theme::accent_selection },
    { "accent_soft", &tw::ui::theme::accent_soft },
    { "accent_hover", &tw::ui::theme::accent_hover },
    { "accent_pressed", &tw::ui::theme::accent_pressed },
    { "accent_border", &tw::ui::theme::accent_border },
    { "accent_selected", &tw::ui::theme::accent_selected },
    { "accent_ghost", &tw::ui::theme::accent_ghost },
    { "surface", &tw::ui::theme::surface },
    { "surface_muted", &tw::ui::theme::surface_muted },
    { "surface_soft", &tw::ui::theme::surface_soft },
    { "surface_hover", &tw::ui::theme::surface_hover },
    { "surface_input", &tw::ui::theme::surface_input },
    { "surface_row", &tw::ui::theme::surface_row },
    { "surface_row_hover", &tw::ui::theme::surface_row_hover },
    { "surface_skeleton", &tw::ui::theme::surface_skeleton },
    { "surface_badge", &tw::ui::theme::surface_badge },
    { "surface_elevated", &tw::ui::theme::surface_elevated },
    { "control_track_off", &tw::ui::theme::control_track_off },
    { "control_thumb", &tw::ui::theme::control_thumb },
    { "border", &tw::ui::theme::border },
    { "border_subtle", &tw::ui::theme::border_subtle },
    { "border_strong", &tw::ui::theme::border_strong },
    { "border_divider", &tw::ui::theme::border_divider },
    { "border_window", &tw::ui::theme::border_window },
    { "text_primary", &tw::ui::theme::text_primary },
    { "text_secondary", &tw::ui::theme::text_secondary },
    { "text_muted", &tw::ui::theme::text_muted },
    { "text_subtle", &tw::ui::theme::text_subtle },
    { "text_faint", &tw::ui::theme::text_faint },
    { "text_glyph_muted", &tw::ui::theme::text_glyph_muted },
    { "text_on_accent", &tw::ui::theme::text_on_accent },
    { "text_error", &tw::ui::theme::text_error },
    { "text_warning", &tw::ui::theme::text_warning },
};

// The order here IS the ABI: the bootstrap chunk indexes this array positionally. Adding an entry
// means appending, never inserting.
void* g_entry_points[] = {
    reinterpret_cast<void*>(&tw::lua::api::tw_channel_resolve),
    reinterpret_cast<void*>(&tw::lua::api::tw_channel_get),
    reinterpret_cast<void*>(&tw::lua::api::tw_engine_ready),
    reinterpret_cast<void*>(&tw::lua::api::tw_log),
    reinterpret_cast<void*>(&tw::lua::api::tw_notify),
    reinterpret_cast<void*>(&tw::lua::api::tw_hud_text),
    reinterpret_cast<void*>(&tw::lua::api::tw_hud_metric),
    reinterpret_cast<void*>(&tw::lua::api::tw_channel_text),
    reinterpret_cast<void*>(&tw::lua::api::tw_kind_name),
    reinterpret_cast<void*>(&tw::lua::api::tw_array_read),
    reinterpret_cast<void*>(&tw::lua::api::tw_on_call),
    reinterpret_cast<void*>(&tw::lua::api::tw_channel_set),
    reinterpret_cast<void*>(&tw::lua::api::tw_group_count),
    reinterpret_cast<void*>(&tw::lua::api::tw_group_name),
    reinterpret_cast<void*>(&tw::lua::api::tw_channel_resolve_at),
    reinterpret_cast<void*>(&tw::lua::api::tw_on_call_at),
    reinterpret_cast<void*>(&tw::lua::api::tw_channel_vector),
    reinterpret_cast<void*>(&tw::lua::api::tw_hud_text_sized),
    reinterpret_cast<void*>(&tw::lua::api::tw_hud_measure),
    reinterpret_cast<void*>(&tw::lua::api::tw_hud_rect),
    reinterpret_cast<void*>(&tw::lua::api::tw_hud_line),
    reinterpret_cast<void*>(&tw::lua::api::tw_hud_widget_rect),
    reinterpret_cast<void*>(&tw::lua::api::tw_mute),
    reinterpret_cast<void*>(&tw::lua::api::tw_mute_at),
    reinterpret_cast<void*>(&tw::lua::api::tw_mute_set),
    reinterpret_cast<void*>(&tw::lua::api::tw_array_read_vector),
    reinterpret_cast<void*>(&tw::lua::api::tw_theme_count),
    reinterpret_cast<void*>(&tw::lua::api::tw_theme_name),
    reinterpret_cast<void*>(&tw::lua::api::tw_theme_color),
    reinterpret_cast<void*>(&tw::lua::api::tw_channel_set_vector),
    reinterpret_cast<void*>(&tw::lua::api::tw_can_write),
    reinterpret_cast<void*>(&tw::lua::api::tw_array_write),
    reinterpret_cast<void*>(&tw::lua::api::tw_array_write_vector),
    reinterpret_cast<void*>(&tw::lua::api::tw_array_rows),
    reinterpret_cast<void*>(&tw::lua::api::tw_font_count),
    reinterpret_cast<void*>(&tw::lua::api::tw_font_name),
    reinterpret_cast<void*>(&tw::lua::api::tw_hud_text_font),
    reinterpret_cast<void*>(&tw::lua::api::tw_hud_measure_font),
    reinterpret_cast<void*>(&tw::lua::api::tw_hud_rect_corners),
    reinterpret_cast<void*>(&tw::lua::api::tw_hud_rect_glow),
    reinterpret_cast<void*>(&tw::lua::api::tw_hud_text_glow),
    reinterpret_cast<void*>(&tw::lua::api::tw_hud_icon),
    reinterpret_cast<void*>(&tw::lua::api::tw_dt),
    reinterpret_cast<void*>(&tw::lua::api::tw_ease),
    reinterpret_cast<void*>(&tw::lua::api::tw_ease_count),
    reinterpret_cast<void*>(&tw::lua::api::tw_ease_name),
    reinterpret_cast<void*>(&tw::lua::api::tw_hud_rect_gradient),
    reinterpret_cast<void*>(&tw::lua::api::tw_frame),
    reinterpret_cast<void*>(&tw::lua::api::tw_state),
    reinterpret_cast<void*>(&tw::lua::api::tw_state_name),
    reinterpret_cast<void*>(&tw::lua::api::tw_ready),
    reinterpret_cast<void*>(&tw::lua::api::tw_graph_revision),
    reinterpret_cast<void*>(&tw::lua::api::tw_group_loaded),
    reinterpret_cast<void*>(&tw::lua::api::tw_ref_free),
    reinterpret_cast<void*>(&tw::lua::api::tw_channel_live),
    reinterpret_cast<void*>(&tw::lua::api::tw_channel_matrix),
    reinterpret_cast<void*>(&tw::lua::api::tw_channel_set_matrix),
};
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

int tw_theme_count() noexcept
{
    return static_cast<int>(std::size(g_theme));
}

const char* tw_theme_name(int index) noexcept
{
    if(index < 0 || index >= static_cast<int>(std::size(g_theme))) {
        return "";
    }

    return g_theme[index].name;
}

unsigned int tw_theme_color(int index) noexcept
{
    if(index < 0 || index >= static_cast<int>(std::size(g_theme))) {
        return 0;
    }

    // Read live rather than cached: the Settings colour pickers edit these in place, and a script
    // that matched the overlay once at load would drift away from it the moment a theme changed.
    return tw::ui::widgets::detail::to_u32(*g_theme[index].color);
}

int tw_on_call(int owner, const char* group_name, const char* channel_name, int after, int* out) noexcept
{
    return resolve_and_subscribe(group_name, channel_name, 0, owner, after, false, out);
}

int tw_group_count() noexcept
{
    return tw::engine::groups::roster_size();
}

const char* tw_group_name(int index) noexcept
{
    // "<pool name> | <file name>", out of the registry's roster. Module-owned, valid until the next
    // call - Lua copies it into a string on the way through the FFI.
    static std::string buffer;

    A3d_ChannelGroup* const group = tw::engine::groups::roster_at(index);
    if(group == nullptr) {
        buffer.clear();
        return buffer.c_str();
    }

    buffer.assign(tw::engine::groups::pool_name_of(group));
    buffer.append(" | ");
    buffer.append(tw::engine::groups::file_name_of(group));

    return buffer.c_str();
}

int tw_on_call_at(int owner, const char* group_name, int index, int after, int* out) noexcept
{
    return resolve_and_subscribe(group_name, nullptr, index, owner, after, false, out);
}

int tw_mute(int owner, const char* group_name, const char* channel_name, int* out) noexcept
{
    return resolve_and_subscribe(group_name, channel_name, 0, owner, 0, true, out);
}

int tw_mute_at(int owner, const char* group_name, int index, int* out) noexcept
{
    return resolve_and_subscribe(group_name, nullptr, index, owner, 0, true, out);
}

void tw_mute_set(int id, int enable) noexcept
{
    subscription* record = find_subscription(id);
    if(record == nullptr || !record->mute) {
        return;
    }

    record->enabled = enable != 0;
}

void tw_unsubscribe_owner(int owner) noexcept
{
    // Two passes, because the shim keys on the record pointer and the records have to outlive the
    // detach: collect first, detach, then free.
    std::vector<void*> doomed;
    for(subscription* record : g_subscriptions) {
        if(record->owner == owner) {
            doomed.push_back(record);
        }
    }

    if(doomed.empty()) {
        return;
    }

    tw::framework::channel_shim::unsubscribe_all_of(doomed);

    std::erase_if(g_subscriptions, [owner](const subscription* record) { return record->owner == owner; });

    for(void* record : doomed) {
        delete static_cast<subscription*>(record);
    }
}

void tw_on_call_clear() noexcept
{
    tw::framework::channel_shim::remove_all();

    for(subscription* record : g_subscriptions) {
        delete record;
    }
    g_subscriptions.clear();
}

int tw_subscription_count(int owner) noexcept
{
    int total = 0;
    for(const subscription* record : g_subscriptions) {
        if(owner < 0 || record->owner == owner) {
            ++total;
        }
    }

    return total;
}

int tw_shared_channel_count() noexcept
{
    // Distinct channels more than one subscription is attached to. Surfaced in the Scripts tab
    // because it is the one thing about a pile of third-party scripts that stays invisible until it
    // misbehaves: two of them quietly watching - or muting - the same node.
    std::vector<const A3d_Channel*> seen;
    for(const subscription* record : g_subscriptions) {
        if(tw::framework::channel_shim::subscriber_count(record->channel) <= 1) {
            continue;
        }
        if(std::ranges::find(seen, record->channel) == seen.end()) {
            seen.push_back(record->channel);
        }
    }

    return static_cast<int>(seen.size());
}

void install_engine_listeners() noexcept
{
    tw::engine::groups::subscribe_unloading(&on_group_unloading);
    tw::engine::groups::subscribe_changed(&on_groups_changed);
}

void tick() noexcept
{
    // All that is left of what used to be the write gate. The gate itself moved to engine::state,
    // where it is one term of a state machine rather than a latch of its own.
    ++g_draw_frames;
}

int tw_can_write() noexcept
{
    return writes_allowed() ? 1 : 0;
}

int tw_state() noexcept
{
    return static_cast<int>(tw::engine::state::current());
}

const char* tw_state_name() noexcept
{
    return tw::engine::state::current_name();
}

int tw_ready() noexcept
{
    return tw::engine::state::ready() ? 1 : 0;
}

int tw_group_loaded(const char* name) noexcept
{
    return tw::engine::groups::find(name) != nullptr ? 1 : 0;
}

int tw_graph_revision() noexcept
{
    // Degraded path first, and it matters: with no group registry there is no revision to report,
    // and a handle comparing a number that never changes would try to resolve once and then give up
    // for the session. Falling back to a frame-derived value reproduces exactly the old
    // retry-every-60-frames behaviour for a session where the registry could not be built.
    if(!tw::engine::groups::available()) [[unlikely]] {
        return tw_frame() / 60;
    }

    // One number that changes exactly when re-resolving a handle could produce a different answer:
    // a group appeared, or one went away. A script's handle compares it instead of counting frames,
    // which is what turns "rescan the whole group every 60 frames forever" into "rescan when
    // something actually changed" (lua-engine-fix-roadmap.md §5.2).
    return static_cast<int>(tw::engine::groups::revision());
}

int tw_engine_ready() noexcept
{
    return tw::engine::channels::available() ? 1 : 0;
}

int tw_frame() noexcept
{
    const std::uint32_t candidate = tw::engine::frame::started() ? tw::engine::frame::count() : g_draw_frames;

    // Monotonic clamp, not a max() for its own sake: the handover from the draw counter to the
    // engine counter happens mid-session and the two are unrelated numbers, so without this a script
    // holding `retry_at = tw.frame + 60` would stop retrying until the engine caught up.
    if(candidate > g_frame) {
        g_frame = candidate;
    }

    return static_cast<int>(g_frame);
}

float tw_dt() noexcept
{
    if(!imgui_ready()) {
        return 0.f;
    }

    // Milliseconds from the overlay's own frame clock rather than ImGui::GetIO().DeltaTime, so a
    // script's animation and an overlay widget's animation cannot disagree about how long a frame
    // was. dt_ms carries the sub-millisecond remainder forward, which is the whole reason it exists.
    return static_cast<float>(tw::ui::widgets::detail::dt_ms()) * 0.001f;
}

float tw_ease(int curve, float t) noexcept
{
    t = std::clamp(t, 0.f, 1.f);

    if(curve < 0 || curve >= static_cast<int>(std::size(g_ease_curves))) {
        return t;
    }

    return g_ease_curves[curve].run(t);
}

int tw_ease_count() noexcept
{
    return static_cast<int>(std::size(g_ease_curves));
}

const char* tw_ease_name(int index) noexcept
{
    if(index < 0 || index >= tw_ease_count()) {
        return nullptr;
    }

    return g_ease_curves[index].name;
}

void tw_log(const char* message) noexcept
{
    if(message == nullptr) {
        return;
    }

    TW_LOG_INFO("lua: {}", message);
}

void tw_notify(const char* message) noexcept
{
    if(message == nullptr) {
        return;
    }

    // notefeed::push takes a string_view and copies into its own storage, so the pointer does not
    // have to outlive the call - which matters, because it points into a Lua string the collector
    // owns.
    tw::ui::plugins::statics::notefeed::push(message);
}

void tw_hud_text(float x, float y, unsigned int color, const char* text) noexcept
{
    if(text == nullptr || !inside_frame()) {
        return;
    }

    // Background draw list rather than a window: no chrome, no input, and it is drawn whether or not
    // the menu is open - which is what a HUD wants. See Docs/Internal/lua-scripting.md §8.3.
    ImGui::GetBackgroundDrawList()->AddText(ImVec2(x, y), color, text);
}

void tw_hud_text_sized(float x, float y, unsigned int color, const char* text, float size) noexcept
{
    if(text == nullptr || !inside_frame()) {
        return;
    }

    ImFont* font = ImGui::GetFont();
    if(font == nullptr) [[unlikely]] {
        return;
    }

    if(size <= 0.f) {
        size = ImGui::GetFontSize();
    }

    ImGui::GetBackgroundDrawList()->AddText(font, size, ImVec2(x, y), color, text);
}

void tw_hud_measure(const char* text, float size, float* out) noexcept
{
    if(out == nullptr) {
        return;
    }

    out[0] = 0.f;
    out[1] = 0.f;

    if(text == nullptr || !imgui_ready()) {
        return;
    }

    ImFont* font = ImGui::GetFont();
    if(font == nullptr) {
        return;
    }

    if(size <= 0.f) {
        size = ImGui::GetFontSize();
    }

    const ImVec2 measured = font->CalcTextSizeA(size, std::numeric_limits<float>::max(), 0.f, text);
    out[0] = measured.x;
    out[1] = measured.y;
}

void tw_hud_rect(float x0, float y0, float x1, float y1, unsigned int color, float rounding, float thickness) noexcept
{
    tw_hud_rect_corners(x0, y0, x1, y1, color, rounding, thickness, hud_corner_all);
}

void tw_hud_rect_corners(
    float x0, float y0, float x1, float y1, unsigned int color, float rounding, float thickness, int corners) noexcept
{
    if(!inside_frame()) {
        return;
    }

    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const ImDrawFlags flags = to_draw_flags(corners);

    if(thickness <= 0.f) {
        draw->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), color, rounding, flags);
    }
    else {
        draw->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), color, rounding, flags, thickness);
    }
}

void tw_hud_rect_glow(float x0, float y0, float x1, float y1, unsigned int color, float rounding, float strength) noexcept
{
    if(!inside_frame()) {
        return;
    }

    tw::ui::widgets::detail::add_rect_glow(
        ImGui::GetBackgroundDrawList(), ImVec2(x0, y0), ImVec2(x1, y1), rounding, ImGui::ColorConvertU32ToFloat4(color), strength);
}

void tw_hud_rect_gradient(
    float x0, float y0, float x1, float y1, unsigned int from, unsigned int to, int vertical) noexcept
{
    if(!inside_frame()) {
        return;
    }

    // Corner order is upper-left, upper-right, lower-right, lower-left. Horizontal puts `from` on
    // the two left corners; vertical puts it on the two top ones.
    const unsigned int tl = from;
    const unsigned int tr = (vertical != 0) ? from : to;
    const unsigned int br = to;
    const unsigned int bl = (vertical != 0) ? to : from;

    ImGui::GetBackgroundDrawList()->AddRectFilledMultiColor(ImVec2(x0, y0), ImVec2(x1, y1), tl, tr, br, bl);
}

void tw_hud_text_glow(float x,
    float y,
    unsigned int text_color,
    unsigned int glow_color,
    const char* text,
    float size,
    int font,
    float strength) noexcept
{
    if(text == nullptr || !inside_frame()) {
        return;
    }

    ImFont* face = tw::ui::fonts::at(font);
    if(face == nullptr) [[unlikely]] {
        return;
    }

    tw::ui::widgets::detail::add_text_glow(ImGui::GetBackgroundDrawList(),
        ImVec2(x, y),
        text,
        text_color,
        ImGui::ColorConvertU32ToFloat4(glow_color),
        strength,
        face,
        size > 0.f ? size : ImGui::GetFontSize());
}

int tw_font_count() noexcept
{
    return tw::ui::fonts::count();
}

const char* tw_font_name(int index) noexcept
{
    return tw::ui::fonts::name(index);
}

void tw_hud_text_font(float x, float y, unsigned int color, const char* text, float size, int font) noexcept
{
    if(text == nullptr || !inside_frame()) {
        return;
    }

    ImFont* face = tw::ui::fonts::at(font);
    if(face == nullptr) [[unlikely]] {
        return;
    }

    ImGui::GetBackgroundDrawList()->AddText(face, size > 0.f ? size : ImGui::GetFontSize(), ImVec2(x, y), color, text);
}

void tw_hud_measure_font(const char* text, float size, int font, float* out) noexcept
{
    if(out == nullptr) {
        return;
    }

    out[0] = 0.f;
    out[1] = 0.f;

    if(text == nullptr || !imgui_ready()) {
        return;
    }

    ImFont* face = tw::ui::fonts::at(font);
    if(face == nullptr) {
        return;
    }

    if(size <= 0.f) {
        size = ImGui::GetFontSize();
    }

    const ImVec2 measured = face->CalcTextSizeA(size, std::numeric_limits<float>::max(), 0.f, text);
    out[0] = measured.x;
    out[1] = measured.y;
}

void tw_hud_icon(const char* name, float x, float y, float size, unsigned int tint) noexcept
{
    if(name == nullptr || size <= 0.f || !inside_frame()) {
        return;
    }

    // Built here, not taken from the script: this is the only thing keeping "icons/*.svg" the only
    // resource class a script can address.
    char key[128];
    if(std::snprintf(key, sizeof(key), "icons/%s.svg", name) < 0) {
        return;
    }

    const tw::ui::image::svg::image icon = tw::ui::image::svg::get_resource(key);
    const ImTextureID texture = icon.at(static_cast<int>(std::lround(size)));
    if(texture == ImTextureID_Invalid) {
        return;
    }

    ImGui::GetBackgroundDrawList()->AddImage(
        ImTextureRef { texture }, ImVec2(x, y), ImVec2(x + size, y + size), ImVec2(0.f, 0.f), ImVec2(1.f, 1.f), tint);
}

void tw_hud_line(float x0, float y0, float x1, float y1, unsigned int color, float thickness) noexcept
{
    if(!inside_frame()) {
        return;
    }

    ImGui::GetBackgroundDrawList()->AddLine(ImVec2(x0, y0), ImVec2(x1, y1), color, thickness <= 0.f ? 1.f : thickness);
}

int tw_hud_widget_rect(int which, float* out) noexcept
{
    if(out == nullptr || !imgui_ready()) {
        return 0;
    }

    float x0 = 0.f;
    float y0 = 0.f;
    float x1 = 0.f;
    float y1 = 0.f;
    bool visible = false;

    switch(which) {
    case hud_widget_notefeed:
        // Always reported, even with no toasts alive: the strip is a reservation, not a measurement
        // (see notefeed::reserved_rect).
        tw::ui::plugins::statics::notefeed::reserved_rect(x0, y0, x1, y1);
        visible = true;
        break;
    case hud_widget_pins:
        visible = tw::ui::plugins::statics::pins::last_rect(x0, y0, x1, y1);
        break;
    case hud_widget_watermark:
        visible = tw::ui::plugins::statics::watermark::last_rect(x0, y0, x1, y1);
        break;
    case hud_widget_menu: {
        ImVec2 pos {};
        ImVec2 size {};
        visible = tw::ui::plugins::interactive::menu::window_rect(pos, size);
        x0 = pos.x;
        y0 = pos.y;
        x1 = pos.x + size.x;
        y1 = pos.y + size.y;
        break;
    }
    default:
        return 0;
    }

    if(!visible) {
        return 0;
    }

    out[0] = x0;
    out[1] = y0;
    out[2] = x1;
    out[3] = y1;

    return 1;
}

float tw_hud_metric(int which) noexcept
{
    if(!imgui_ready()) {
        return 0.f;
    }

    const ImVec2 viewport = ImGui::GetIO().DisplaySize;

    switch(which) {
    case hud_viewport_width:
        return viewport.x;
    case hud_viewport_height:
        return viewport.y;
    case hud_font_size:
        return ImGui::GetFontSize();
    default:
        break;
    }

    // The always-on chrome lives in a band across the top: the toast column occupies one top corner
    // and the watermark the other (see overlay_config::feed_side). So the safe area is the viewport
    // with that band removed - full width, which is what matters.
    //
    // It used to shave the notefeed's whole column off one side instead, back when the feed reserved
    // full height. That made the safe area a tall box offset to one side, and anything laid out
    // against it landed a third of the screen from the corner it was aiming for. Pins are the one
    // piece of chrome this cannot express - they float mid-height on one side, and excluding them
    // would make the safe area a hole rather than a rectangle - so they are left to
    // tw_hud_widget_rect, which reports them exactly.
    float feed_x0 = 0.f;
    float feed_y0 = 0.f;
    float feed_x1 = 0.f;
    float feed_y1 = 0.f;
    tw::ui::plugins::statics::notefeed::reserved_rect(feed_x0, feed_y0, feed_x1, feed_y1);

    float top = feed_y1;

    float mark_x0 = 0.f;
    float mark_y0 = 0.f;
    float mark_x1 = 0.f;
    float mark_y1 = 0.f;
    if(tw::ui::plugins::statics::watermark::last_rect(mark_x0, mark_y0, mark_x1, mark_y1)) {
        top = std::max(top, mark_y1);
    }

    switch(which) {
    case hud_safe_x0:
        return 0.f;
    case hud_safe_y0:
        return top + 8.f;
    case hud_safe_x1:
        return viewport.x;
    case hud_safe_y1:
        return viewport.y;
    default:
        return 0.f;
    }
}
}

std::span<void* const> entry_points() noexcept
{
    return std::span<void* const>(g_entry_points, std::size(g_entry_points));
}
} // namespace tw::lua::api
