// No TweakerPlugin PCH here - shared with smoke_test (see src/ui/CMakeLists.txt).
#include "ui/host_link.hxx"

#include "ui/pending_actions.hxx"
#include "ui/plugins/static/notefeed.hxx"
#include "ui/qp/qp_pending.hxx"

namespace
{
// Render thread only.
bool g_seeded = false;
bool g_last_connected = false;
} // namespace

namespace tw::ui::host_link
{
edge update(const overlay_state::cache& snapshot) noexcept
{
    if(!g_seeded) [[unlikely]] {
        g_seeded = true;
        g_last_connected = snapshot.host_connected;
        return edge::none;
    }

    if(snapshot.host_connected == g_last_connected) [[likely]] {
        return edge::none;
    }

    g_last_connected = snapshot.host_connected;

    tw::ui::pending_actions::reset();
    tw::ui::qp::pending::reset();

    if(snapshot.host_connected) {
        tw::ui::plugins::statics::notefeed::push("Connected to Audiosurf Tweaker");
        return edge::connected;
    }

    tw::ui::plugins::statics::notefeed::push("Audiosurf Tweaker disconnected");
    return edge::disconnected;
}
} // namespace tw::ui::host_link
