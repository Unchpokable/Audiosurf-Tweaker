// No TweakerPlugin PCH here - this TU is shared with smoke_test (see src/ui/CMakeLists.txt) and
// has no D3D9/Quest3D dependency, only STL (pulled in via ui/overlay_state.hxx + the two below).
#include "ui/overlay_state.hxx"

#include <mutex>
#include <utility>

namespace
{
using tw::ui::overlay_state::tweak_id;

// Everything that varies per tweak, in enum order. One table instead of the five parallel switches
// this used to be (wire name, display name, icon, id list, index) - adding a tweak is a line here
// plus an enum member, and the two cannot silently disagree because of the static_assert below.
struct tweak_info {
    tweak_id id;
    std::string_view wire_name;
    std::string_view display_name;
    std::string_view icon_key;
};

constexpr std::array<tweak_info, tw::ui::overlay_state::k_tweak_count> k_tweaks { {
    { tweak_id::invisible_road, "InvisibleRoad", "Invisible road", "icons/tweak_invisible_road.svg" },
    { tweak_id::hidden_song_title, "HiddenSongTitle", "Hidden song title", "icons/tweak_hidden_song_title.svg" },
    { tweak_id::sidewinder_camera, "SidewinderCamera", "Sidewinder camera", "icons/tweak_sidewinder_camera.svg" },
    { tweak_id::banking_camera, "BankingCamera", "Banking camera", "icons/tweak_banking_camera.svg" },
    { tweak_id::freeride_no_blocks, "FreerideNoBlocks", "Freeride: no blocks", "icons/tweak_freeride_no_blocks.svg" },
    { tweak_id::freeride_blocks_caterpillars, "FreerideBlocksCaterpillars", "Freeride: block caterpillars",
        "icons/tweak_freeride_blocks_caterpillars.svg" },
    { tweak_id::freeride_auto_advance_disable, "FreerideAutoAdvanceDisable", "Freeride: disable auto-advance",
        "icons/tweak_freeride_auto_advance_disable.svg" },
} };

// tweak_index() is a plain cast only as long as the table is in enum order; if it ever isn't, every
// lookup below silently returns the wrong tweak.
static_assert([] {
    for(std::size_t i = 0; i < k_tweaks.size(); ++i) {
        if(static_cast<std::size_t>(k_tweaks[i].id) != i) {
            return false;
        }
    }
    return true;
}(), "k_tweaks must be in tweak_id order");

// Single critical section guarding every field below, plus g_generation. The writer (IPC thread)
// takes a normal blocking lock; refresh() (UI thread, once per frame) only ever takes a
// try_lock() - see refresh() for why that split is safe and never stalls the render thread.
std::mutex g_state_mutex;
std::uint32_t g_generation = 0;

std::array<std::uint8_t, tw::ui::overlay_state::k_tweak_count> g_tweak_enabled {};
std::array<std::uint8_t, tw::ui::overlay_state::k_tweak_count> g_tweak_quick_player {};
std::vector<std::string> g_skin_names;
std::string g_current_skin_name;
} // namespace

namespace tw::ui::overlay_state
{
tweak_id resolve_tweak_id(std::string_view wire_name) noexcept
{
    for(const auto& tweak : k_tweaks) {
        if(tweak.wire_name == wire_name) {
            return tweak.id;
        }
    }

    return tweak_id::unknown;
}

std::string_view tweak_display_name(tweak_id id) noexcept
{
    return id == tweak_id::unknown ? std::string_view {} : k_tweaks[tweak_index(id)].display_name;
}

std::string_view tweak_icon_key(tweak_id id) noexcept
{
    return id == tweak_id::unknown ? std::string_view {} : k_tweaks[tweak_index(id)].icon_key;
}

std::string_view skin_icon_key() noexcept
{
    return "icons/skin.svg";
}

std::string_view tweak_wire_name(tweak_id id) noexcept
{
    return id == tweak_id::unknown ? std::string_view {} : k_tweaks[tweak_index(id)].wire_name;
}

const std::array<tweak_id, k_tweak_count>& all_tweak_ids() noexcept
{
    static constexpr auto k_ids = [] {
        std::array<tweak_id, k_tweak_count> ids {};
        for(std::size_t i = 0; i < k_tweaks.size(); ++i) {
            ids[i] = k_tweaks[i].id;
        }
        return ids;
    }();
    return k_ids;
}

void set_tweak(tweak_id id, bool enabled, bool quick_player)
{
    if(id == tweak_id::unknown) {
        return;
    }

    std::lock_guard lock(g_state_mutex);
    const std::size_t index = tweak_index(id);
    g_tweak_enabled[index] = enabled ? 1U : 0U;
    g_tweak_quick_player[index] = quick_player ? 1U : 0U;
    ++g_generation;
}

void set_skin_list(std::vector<std::string> names)
{
    std::lock_guard lock(g_state_mutex);
    g_skin_names = std::move(names);
    ++g_generation;
}

void set_current_skin(std::string name)
{
    std::lock_guard lock(g_state_mutex);
    g_current_skin_name = std::move(name);
    ++g_generation;
}

void reset()
{
    std::lock_guard lock(g_state_mutex);
    g_tweak_enabled = {};
    g_tweak_quick_player = {};
    g_skin_names.clear();
    g_current_skin_name.clear();
    ++g_generation;
}

bool refresh(cache& out)
{
    std::unique_lock lock(g_state_mutex, std::try_to_lock);
    if(!lock.owns_lock()) {
        return false;
    }

    const bool changed = g_generation != out.seen_generation;
    if(changed) {
        out.tweak_enabled = g_tweak_enabled;
        out.tweak_quick_player = g_tweak_quick_player;
        out.skin_names = g_skin_names;
        out.current_skin_name = g_current_skin_name;
        out.seen_generation = g_generation;
    }

    return changed;
}

bool is_tweak_enabled(const cache& snapshot, tweak_id id) noexcept
{
    if(id == tweak_id::unknown) {
        return false;
    }

    return snapshot.tweak_enabled[tweak_index(id)] != 0U;
}

bool is_tweak_quick_player(const cache& snapshot, tweak_id id) noexcept
{
    if(id == tweak_id::unknown) {
        return false;
    }

    return snapshot.tweak_quick_player[tweak_index(id)] != 0U;
}
} // namespace tw::ui::overlay_state
