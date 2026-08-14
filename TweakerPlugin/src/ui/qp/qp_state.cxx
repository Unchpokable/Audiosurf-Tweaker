// No TweakerPlugin PCH here - this TU is shared with smoke_test (see src/ui/CMakeLists.txt) and
// has no D3D9/Quest3D dependency, only STL. Same convention as overlay_state.cxx.
#include "ui/qp/qp_state.hxx"

#include <mutex>
#include <utility>

namespace
{
using tw::ui::qp::state::order_state;
using tw::ui::qp::state::playlist_list;
using tw::ui::qp::state::track_list;

// Single critical section guarding every field below, plus g_generation. The writer (IPC thread)
// takes a normal blocking lock; refresh() (UI thread, once per frame) only ever takes a try_lock -
// see overlay_state.cxx for why that split is safe and never stalls the render thread.
std::mutex g_state_mutex;
std::uint32_t g_generation = 0;

// Published as shared_ptr-to-const and never mutated after publication, so refresh() can hand the
// render thread a pointer instead of deep-copying up to a thousand tracks (see cache in the header).
std::shared_ptr<const playlist_list> g_playlists;
std::string g_tracks_playlist_id;
std::shared_ptr<const track_list> g_tracks;

order_state g_order;
std::string g_playing_playlist_id;
std::string g_playing_entry_id;
tw::ui::qp::character_id g_default_character = tw::ui::qp::character_id::mono;
bool g_connected = false;

// Returned by the null-safe accessors before the first push. Function-local statics rather than
// namespace-scope globals so their construction cannot race the first refresh during static init.
const playlist_list& empty_playlists() noexcept
{
    static const playlist_list k_empty;
    return k_empty;
}

const track_list& empty_tracks() noexcept
{
    static const track_list k_empty;
    return k_empty;
}
} // namespace

namespace tw::ui::qp::state
{
void set_catalog(playlist_list playlists)
{
    auto published = std::make_shared<const playlist_list>(std::move(playlists));

    std::lock_guard lock(g_state_mutex);
    g_playlists = std::move(published);
    g_connected = true;
    ++g_generation;
}

void set_tracks(std::string playlist_id, track_list tracks)
{
    // Allocated outside the lock: building the shared_ptr copies the vector's control block and can
    // allocate, and there is no reason to make refresh() wait through that.
    auto published = std::make_shared<const track_list>(std::move(tracks));

    std::lock_guard lock(g_state_mutex);
    g_tracks_playlist_id = std::move(playlist_id);
    g_tracks = std::move(published);
    ++g_generation;
}

void set_order(order_state value)
{
    std::lock_guard lock(g_state_mutex);
    g_order = std::move(value);
    ++g_generation;
}

void set_now_playing(std::string playlist_id, std::string entry_id)
{
    std::lock_guard lock(g_state_mutex);
    g_playing_playlist_id = std::move(playlist_id);
    g_playing_entry_id = std::move(entry_id);
    ++g_generation;
}

void set_stopped()
{
    std::lock_guard lock(g_state_mutex);
    g_playing_playlist_id.clear();
    g_playing_entry_id.clear();
    ++g_generation;
}

void set_default_character(character_id id)
{
    std::lock_guard lock(g_state_mutex);
    g_default_character = id;
    ++g_generation;
}

void reset()
{
    std::lock_guard lock(g_state_mutex);
    g_playlists.reset();
    g_tracks.reset();
    g_tracks_playlist_id.clear();
    g_order = {};
    g_playing_playlist_id.clear();
    g_playing_entry_id.clear();
    g_default_character = character_id::mono;
    g_connected = false;
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
        out.playlists = g_playlists;
        out.tracks_playlist_id = g_tracks_playlist_id;
        out.tracks = g_tracks;
        out.order = g_order;
        out.playing_playlist_id = g_playing_playlist_id;
        out.playing_entry_id = g_playing_entry_id;
        out.default_character = g_default_character;
        out.connected = g_connected;
        out.seen_generation = g_generation;
    }

    return changed;
}

const playlist_list& playlists_of(const cache& snapshot) noexcept
{
    return snapshot.playlists != nullptr ? *snapshot.playlists : empty_playlists();
}

const track_list& tracks_of(const cache& snapshot) noexcept
{
    return snapshot.tracks != nullptr ? *snapshot.tracks : empty_tracks();
}

const track* find_track(const cache& snapshot, std::string_view entry_id) noexcept
{
    if(entry_id.empty()) {
        return nullptr;
    }

    for(const track& entry : tracks_of(snapshot)) {
        if(entry.entry_id == entry_id) {
            return &entry;
        }
    }

    return nullptr;
}

bool has_mod(const track& entry, overlay_state::tweak_id id) noexcept
{
    for(const overlay_state::tweak_id mod : entry.mods) {
        if(mod == id) {
            return true;
        }
    }

    return false;
}

const tag_state* find_tag(const track& entry, tag_id id) noexcept
{
    for(const tag_state& tag : entry.tags) {
        if(tag.id == id) {
            return &tag;
        }
    }

    return nullptr;
}
} // namespace tw::ui::qp::state
