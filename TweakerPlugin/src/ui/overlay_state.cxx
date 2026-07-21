// No TweakerPlugin PCH here - this TU is shared with smoke_test (see src/ui/CMakeLists.txt) and
// has no D3D9/Quest3D dependency, only STL (pulled in via ui/overlay_state.hxx + the two below).
#include "ui/overlay_state.hxx"

#include <mutex>
#include <utility>

namespace
{
using tw::ui::overlay_state::tweak_id;

constexpr std::string_view k_tweak_invisible_road = "InvisibleRoad";
constexpr std::string_view k_tweak_hidden_song_title = "HiddenSongTitle";
constexpr std::string_view k_tweak_sidewinder_camera = "SidewinderCamera";
constexpr std::string_view k_tweak_banking_camera = "BankingCamera";
constexpr std::string_view k_tweak_freeride_no_blocks = "FreerideNoBlocks";
constexpr std::string_view k_tweak_freeride_blocks_caterpillars = "FreerideBlocksCaterpillars";
constexpr std::string_view k_tweak_freeride_auto_advance_disable = "FreerideAutoAdvanceDisable";

// Single critical section guarding every field below, plus g_generation. The writer (IPC thread)
// takes a normal blocking lock; refresh() (UI thread, once per frame) only ever takes a
// try_lock() - see refresh() for why that split is safe and never stalls the render thread.
std::mutex g_state_mutex;
std::uint32_t g_generation = 0;

std::array<std::uint8_t, tw::ui::overlay_state::k_tweak_count> g_tweak_enabled {};
std::vector<std::string> g_skin_names;
std::string g_current_skin_name;

std::size_t tweak_index(tweak_id id) noexcept
{
    switch(id) {
        case tweak_id::invisible_road: return 0;
        case tweak_id::hidden_song_title: return 1;
        case tweak_id::sidewinder_camera: return 2;
        case tweak_id::banking_camera: return 3;
        case tweak_id::freeride_no_blocks: return 4;
        case tweak_id::freeride_blocks_caterpillars: return 5;
        case tweak_id::freeride_auto_advance_disable: return 6;
        default: return 0;
    }
}
} // namespace

namespace tw::ui::overlay_state
{
tweak_id resolve_tweak_id(std::string_view wire_name) noexcept
{
    if(wire_name == k_tweak_invisible_road) {
        return tweak_id::invisible_road;
    }

    if(wire_name == k_tweak_hidden_song_title) {
        return tweak_id::hidden_song_title;
    }

    if(wire_name == k_tweak_sidewinder_camera) {
        return tweak_id::sidewinder_camera;
    }

    if(wire_name == k_tweak_banking_camera) {
        return tweak_id::banking_camera;
    }

    if(wire_name == k_tweak_freeride_no_blocks) {
        return tweak_id::freeride_no_blocks;
    }

    if(wire_name == k_tweak_freeride_blocks_caterpillars) {
        return tweak_id::freeride_blocks_caterpillars;
    }

    if(wire_name == k_tweak_freeride_auto_advance_disable) {
        return tweak_id::freeride_auto_advance_disable;
    }

    return tweak_id::unknown;
}

std::string_view tweak_display_name(tweak_id id) noexcept
{
    switch(id) {
        case tweak_id::invisible_road: return "Invisible road";
        case tweak_id::hidden_song_title: return "Hidden song title";
        case tweak_id::sidewinder_camera: return "Sidewinder camera";
        case tweak_id::banking_camera: return "Banking camera";
        case tweak_id::freeride_no_blocks: return "Freeride: no blocks";
        case tweak_id::freeride_blocks_caterpillars: return "Freeride: block caterpillars";
        case tweak_id::freeride_auto_advance_disable: return "Freeride: disable auto-advance";
        default: return {};
    }
}

const std::array<tweak_id, k_tweak_count>& all_tweak_ids() noexcept
{
    static constexpr std::array<tweak_id, k_tweak_count> k_ids {
        tweak_id::invisible_road,
        tweak_id::hidden_song_title,
        tweak_id::sidewinder_camera,
        tweak_id::banking_camera,
        tweak_id::freeride_no_blocks,
        tweak_id::freeride_blocks_caterpillars,
        tweak_id::freeride_auto_advance_disable,
    };
    return k_ids;
}

void set_tweak(tweak_id id, bool enabled)
{
    if(id == tweak_id::unknown) {
        return;
    }

    std::lock_guard lock(g_state_mutex);
    g_tweak_enabled[tweak_index(id)] = enabled ? 1U : 0U;
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
} // namespace tw::ui::overlay_state
