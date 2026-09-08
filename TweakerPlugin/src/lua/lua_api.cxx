#include "pch.hxx"

#include "lua/lua_api.hxx"

#include "framework/channel_shim.hxx"

#include "libtweeny/tweeny.hxx"

#include "lua/lua_channels.hxx"
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
    A3d_Channel* channel;
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

// Shared tail of every subscribe path. No kind check on purpose: CallChannel is slot 1 of the *base*
// vtable, so every channel of every family has it - this is the one thing that does not care what
// the channel is.
int subscribe(A3d_Channel* channel, int owner, int after, bool mute, int* out) noexcept
{
    const int id = g_next_subscription_id;

    auto* record = new(std::nothrow) subscription { id, owner, channel, after != 0, mute, true };
    if(record == nullptr) {
        if(out != nullptr) { out[0] = tw::lua::api::resolve_unusable; }
        return -1;
    }

    // The record doubles as the subscriber key: it is unique per subscription, which is exactly what
    // the shim needs to tell two scripts watching the same channel apart.
    if(!tw::framework::channel_shim::subscribe(channel, &dispatch_channel_call, record)) {
        delete record;
        if(out != nullptr) { out[0] = tw::lua::api::resolve_unusable; }
        return -1;
    }

    g_subscriptions.push_back(record);
    ++g_next_subscription_id;

    if(out != nullptr) { out[0] = tw::lua::api::resolve_ok; }
    return id;
}

// Resolve-and-subscribe, shared by the by-name and by-index forms of on_call and mute. Returns -1
// with a status in `out` when the group or channel is not there yet, which is a normal startup state
// and gets retried from Lua.
int resolve_and_subscribe(
    const char* group_name, const char* channel_name, int index, int owner, int after, bool mute, int* out) noexcept
{
    const auto set = [out](tw::lua::api::resolve_status status) {
        if(out != nullptr) {
            out[0] = status;
            out[1] = static_cast<int>(tw::lua::channels::kind::unknown);
        }
    };

    if(!tw::lua::channels::is_ready() || !tw::lua::channels::has_engine()) {
        set(tw::lua::api::resolve_engine_pending);
        return -1;
    }

    A3d_ChannelGroup* group = tw::lua::channels::find_group(group_name);
    if(group == nullptr) {
        set(tw::lua::api::resolve_no_group);
        return -1;
    }

    A3d_Channel* channel = channel_name != nullptr ? tw::lua::channels::find_channel(group, channel_name)
                                                   : tw::lua::channels::find_channel_at(group, index);
    if(channel == nullptr) {
        set(tw::lua::api::resolve_no_channel);
        return -1;
    }

    return subscribe(channel, owner, after, mute, out);
}

// Whether the game has finished coming up far enough to be written to.
//
// **This exists because of a failure mode that does not announce itself.** Writing into the graph
// while the game is still loading does not crash it - it makes it quietly wrong. Observed: a broken
// track generator, and characters swapped around in the menu, from a script whose only stated job was
// recolouring tiles. Nothing connects the symptom to the cause, and the player has no way to guess.
// A script injected early is exactly the case that hits this, because its first frames land in the
// middle of the game assembling itself.
//
// **What the signal must not be: a game state.** The obvious gate is "StartupState == State_MainMenu",
// and it is wrong twice over. StartupState (#855) is a leaf whose stored value *is* 2, the same number
// as State_MainMenu - so it reads "in the menu" from the instant XX_StartHere loads, which is the
// middle of the window this is supposed to protect. And latching on any particular state also breaks
// the common case of injecting into an already-running game: sitting in the song selector, the game
// would never present the state being waited for, and writes would stay blocked until the player
// happened to walk back to the menu.
//
// **What it is instead: the game has stopped assembling itself.** Loading is exactly the period when
// channel groups are being added, so a group count that has not moved for a while means loading has
// finished - and that is true whether we were injected before the game started or into a session
// already in progress. No table of state values, nothing to keep in step with the game.
//
// This is a heuristic, and it is worth being plain about that. It says "nothing has loaded recently",
// not "the game is definitely ready". A latch, so a later transition (entering a run does move groups
// around) cannot close it again.
constexpr float k_settle_seconds = 1.0f;

bool g_write_gate_open = false;
bool g_write_gate_warned = false;
int g_last_group_count = -1;
float g_stable_seconds = 0.f;

[[nodiscard]] bool writes_allowed() noexcept
{
    return g_write_gate_open;
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
};
} // namespace

namespace tw::lua::api
{
extern "C" {
void* tw_channel_resolve(const char* group_name, const char* channel_name, int wanted_kind, int* out) noexcept
{
    const auto set = [out](resolve_status status, tw::lua::channels::kind actual) {
        if(out != nullptr) {
            out[0] = status;
            out[1] = static_cast<int>(actual);
        }
    };

    if(!tw::lua::channels::is_ready() || !tw::lua::channels::has_engine()) {
        set(resolve_engine_pending, tw::lua::channels::kind::unknown);
        return nullptr;
    }

    A3d_ChannelGroup* group = tw::lua::channels::find_group(group_name);
    if(group == nullptr) {
        set(resolve_no_group, tw::lua::channels::kind::unknown);
        return nullptr;
    }

    A3d_Channel* channel = tw::lua::channels::find_channel(group, channel_name);
    if(channel == nullptr) {
        set(resolve_no_channel, tw::lua::channels::kind::unknown);
        return nullptr;
    }

    // The gate that keeps a wrong accessor from becoming a wild call: the same vtable slot means
    // GetFloat on one family and GetString on another, and on a family with no virtuals of its own it
    // is past the end of the table entirely. See lua_channels::kind_of.
    const tw::lua::channels::kind actual = tw::lua::channels::kind_of(channel);
    if(actual != static_cast<tw::lua::channels::kind>(wanted_kind)) {
        set(resolve_wrong_kind, actual);
        return nullptr;
    }

    if(!tw::lua::channels::is_callable_as(channel, actual)) {
        set(resolve_unusable, actual);
        return nullptr;
    }

    set(resolve_ok, actual);
    return channel;
}

void* tw_channel_resolve_at(const char* group_name, int index, int wanted_kind, int* out) noexcept
{
    const auto set = [out](resolve_status status, tw::lua::channels::kind actual) {
        if(out != nullptr) {
            out[0] = status;
            out[1] = static_cast<int>(actual);
        }
    };

    if(!tw::lua::channels::is_ready() || !tw::lua::channels::has_engine()) {
        set(resolve_engine_pending, tw::lua::channels::kind::unknown);
        return nullptr;
    }

    A3d_ChannelGroup* group = tw::lua::channels::find_group(group_name);
    if(group == nullptr) {
        set(resolve_no_group, tw::lua::channels::kind::unknown);
        return nullptr;
    }

    A3d_Channel* channel = tw::lua::channels::find_channel_at(group, index);
    if(channel == nullptr) {
        set(resolve_no_channel, tw::lua::channels::kind::unknown);
        return nullptr;
    }

    const tw::lua::channels::kind actual = tw::lua::channels::kind_of(channel);
    if(actual != static_cast<tw::lua::channels::kind>(wanted_kind)) {
        set(resolve_wrong_kind, actual);
        return nullptr;
    }

    if(!tw::lua::channels::is_callable_as(channel, actual)) {
        set(resolve_unusable, actual);
        return nullptr;
    }

    set(resolve_ok, actual);
    return channel;
}

const char* tw_kind_name(int kind) noexcept
{
    return tw::lua::channels::kind_name(static_cast<tw::lua::channels::kind>(kind));
}

float tw_channel_get(void* channel) noexcept
{
    return tw::lua::channels::get_float(static_cast<A3d_Channel*>(channel));
}

void tw_channel_set(void* channel, float value) noexcept
{
    if(!writes_allowed()) {
        report_write_refused();
        return;
    }

    tw::lua::channels::set_float(static_cast<A3d_Channel*>(channel), value);
}

void tw_channel_set_vector(void* channel, float x, float y, float z) noexcept
{
    if(!writes_allowed()) {
        report_write_refused();
        return;
    }

    tw::lua::channels::set_vector(static_cast<A3d_Channel*>(channel), x, y, z);
}

const char* tw_channel_text(void* channel) noexcept
{
    return tw::lua::channels::get_text(static_cast<A3d_Channel*>(channel));
}

int tw_channel_vector(void* channel, float* out) noexcept
{
    if(out == nullptr) {
        return 0;
    }

    return tw::lua::channels::get_vector(static_cast<A3d_Channel*>(channel), out) ? 1 : 0;
}

float tw_array_read(void* array_value, void* indexer, float index) noexcept
{
    return tw::lua::channels::read_array(static_cast<A3d_Channel*>(array_value), static_cast<A3d_Channel*>(indexer), index);
}

int tw_array_read_vector(void* array_vector, void* indexer, float index, float* out) noexcept
{
    if(out == nullptr) {
        return 0;
    }

    return tw::lua::channels::read_array_vector(
               static_cast<A3d_Channel*>(array_vector), static_cast<A3d_Channel*>(indexer), index, out)
        ? 1
        : 0;
}

int tw_array_write(void* array_value, void* indexer, float index, float value) noexcept
{
    if(!writes_allowed()) {
        report_write_refused();
        return 0;
    }

    return tw::lua::channels::write_array(static_cast<A3d_Channel*>(array_value), static_cast<A3d_Channel*>(indexer), index, value)
        ? 1
        : 0;
}

int tw_array_write_vector(void* array_vector, void* indexer, float index, float x, float y, float z) noexcept
{
    if(!writes_allowed()) {
        report_write_refused();
        return 0;
    }

    return tw::lua::channels::write_array_vector(
               static_cast<A3d_Channel*>(array_vector), static_cast<A3d_Channel*>(indexer), index, x, y, z)
        ? 1
        : 0;
}

int tw_array_rows(void* column) noexcept
{
    return tw::lua::channels::array_row_count(static_cast<A3d_Channel*>(column));
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
    return tw::lua::channels::group_count();
}

const char* tw_group_name(int index) noexcept
{
    return tw::lua::channels::group_describe(index);
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

void tick() noexcept
{
    if(g_write_gate_open) [[likely]] {
        return;
    }

    if(!tw::lua::channels::is_ready() || !tw::lua::channels::has_engine()) {
        g_stable_seconds = 0.f;
        return;
    }

    const int count = tw::lua::channels::group_count();
    if(count != g_last_group_count) {
        g_last_group_count = count;
        g_stable_seconds = 0.f;
        return;
    }

    g_stable_seconds += ImGui::GetIO().DeltaTime;
    if(g_stable_seconds < k_settle_seconds) {
        return;
    }

    g_write_gate_open = true;
    TW_LOG_INFO("lua_api: write gate open - {} channel group(s) loaded and stable for {:.1f}s", count, k_settle_seconds);
}

int tw_can_write() noexcept
{
    return writes_allowed() ? 1 : 0;
}

int tw_engine_ready() noexcept
{
    return (tw::lua::channels::is_ready() && tw::lua::channels::has_engine()) ? 1 : 0;
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
