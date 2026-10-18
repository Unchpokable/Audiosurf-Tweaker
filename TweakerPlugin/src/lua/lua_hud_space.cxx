#include "pch.hxx"

#include "lua/lua_hud_space.hxx"

namespace
{
using tw::lua::hud_space::entry;
using tw::lua::hud_space::rect;

constexpr int k_boxes = tw::lua::hud_space::k_boxes_per_script;
constexpr float k_gap = tw::lua::hud_space::k_merge_gap;

// One script's boxes. `stamp` says which frame they belong to: a script whose stamp is old has drawn
// nothing this frame, and begin_frame() does not have to touch every script to say so.
struct space {
    std::uint32_t stamp = 0;
    int count = 0;
    std::array<rect, k_boxes> boxes {};
};

std::vector<space> g_spaces;

// Scripts in the order they first drew this frame - the run order, since on_frame runs in it.
std::vector<int> g_drawn;

constexpr std::size_t k_chrome = 8;
std::array<entry, k_chrome> g_chrome {};
std::size_t g_chrome_count = 0;

rect g_screen {};
std::uint32_t g_stamp = 1;

[[nodiscard]] rect join(const rect& a, const rect& b) noexcept
{
    return { std::min(a.x0, b.x0), std::min(a.y0, b.y0), std::max(a.x1, b.x1), std::max(a.y1, b.y1) };
}

[[nodiscard]] float area(const rect& a) noexcept
{
    return (a.x1 - a.x0) * (a.y1 - a.y0);
}

// Overlapping, or closer than `gap` on both axes.
[[nodiscard]] bool within(const rect& a, const rect& b, float gap) noexcept
{
    return a.x0 - gap <= b.x1 && b.x0 - gap <= a.x1 && a.y0 - gap <= b.y1 && b.y0 - gap <= a.y1;
}

// Strictly overlapping once `b` is grown by `margin`: a box exactly `margin` away does not count.
[[nodiscard]] bool hits(const rect& a, const rect& b, float margin) noexcept
{
    return a.x0 < b.x1 + margin && b.x0 - margin < a.x1 && a.y0 < b.y1 + margin && b.y0 - margin < a.y1;
}

// Adds a box to one script's set: merged into every box it is near - repeatedly, since a merged box can
// reach one that the original did not - and when the set is full, into whichever box grows least by it.
void add(space& set, rect box) noexcept
{
    for(int i = 0; i < set.count;) {
        if(within(set.boxes[static_cast<std::size_t>(i)], box, k_gap)) {
            box = join(box, set.boxes[static_cast<std::size_t>(i)]);
            set.boxes[static_cast<std::size_t>(i)] = set.boxes[static_cast<std::size_t>(--set.count)];
            i = 0;
            continue;
        }
        ++i;
    }

    if(set.count < k_boxes) {
        set.boxes[static_cast<std::size_t>(set.count++)] = box;
        return;
    }

    int best = 0;
    float best_growth = std::numeric_limits<float>::max();
    for(int i = 0; i < set.count; ++i) {
        const rect& current = set.boxes[static_cast<std::size_t>(i)];
        const float growth = area(join(current, box)) - area(current);
        if(growth < best_growth) {
            best_growth = growth;
            best = i;
        }
    }
    set.boxes[static_cast<std::size_t>(best)] = join(set.boxes[static_cast<std::size_t>(best)], box);
}

[[nodiscard]] bool current(int id) noexcept
{
    return id >= 0 && id < static_cast<int>(g_spaces.size()) && g_spaces[static_cast<std::size_t>(id)].stamp == g_stamp;
}

// Whether a box for place() collides with anything it has to stay clear of.
[[nodiscard]] bool blocked(const rect& box, float margin) noexcept
{
    for(std::size_t i = 0; i < g_chrome_count; ++i) {
        if(g_chrome[i].by != tw::lua::hud_space::chrome_menu && hits(box, g_chrome[i].box, margin)) {
            return true;
        }
    }

    for(const int id : g_drawn) {
        const space& set = g_spaces[static_cast<std::size_t>(id)];
        for(int i = 0; i < set.count; ++i) {
            if(hits(box, set.boxes[static_cast<std::size_t>(i)], margin)) {
                return true;
            }
        }
    }

    return false;
}
} // namespace

namespace tw::lua::hud_space
{
void begin_frame(rect screen, std::span<const entry> chrome, int scripts)
{
    ++g_stamp;
    g_screen = screen;
    g_drawn.clear();

    if(scripts > static_cast<int>(g_spaces.size())) {
        g_spaces.resize(static_cast<std::size_t>(scripts));
    }
    // Capacity for every script to appear once: note() pushes here, and must not allocate.
    if(g_drawn.capacity() < g_spaces.size()) {
        g_drawn.reserve(g_spaces.size());
    }

    g_chrome_count = 0;
    for(const entry& item : chrome) {
        if(g_chrome_count == k_chrome) {
            break;
        }
        g_chrome[g_chrome_count++] = item;
    }
}

void note(int owner, rect box) noexcept
{
    if(owner < 0 || owner >= static_cast<int>(g_spaces.size())) [[unlikely]] {
        return;
    }

    box.x0 = std::max(box.x0, g_screen.x0);
    box.y0 = std::max(box.y0, g_screen.y0);
    box.x1 = std::min(box.x1, g_screen.x1);
    box.y1 = std::min(box.y1, g_screen.y1);

    // Written as "not greater" so that NaN, which compares false to everything, is dropped too.
    if(!(box.x1 > box.x0) || !(box.y1 > box.y0)) [[unlikely]] {
        return;
    }

    space& set = g_spaces[static_cast<std::size_t>(owner)];
    if(set.stamp != g_stamp) {
        set.stamp = g_stamp;
        set.count = 0;
        if(g_drawn.size() < g_drawn.capacity()) [[likely]] {
            g_drawn.push_back(owner);
        }
    }

    add(set, box);
}

int occupied(int owner, std::span<entry> out) noexcept
{
    std::size_t written = 0;

    for(std::size_t i = 0; i < g_chrome_count && written < out.size(); ++i) {
        out[written++] = g_chrome[i];
    }

    for(const int id : g_drawn) {
        if(id == owner) {
            continue;
        }
        const space& set = g_spaces[static_cast<std::size_t>(id)];
        for(int i = 0; i < set.count && written < out.size(); ++i) {
            out[written++] = entry { set.boxes[static_cast<std::size_t>(i)], id };
        }
    }

    return static_cast<int>(written);
}

bool place(int owner, float width, float height, corner where, float margin, float* out) noexcept
{
    if(out == nullptr || !(width > 0.f) || !(height > 0.f) || !(margin >= 0.f)) {
        return false;
    }

    const bool right = where == corner_top_right || where == corner_bottom_right;
    const bool bottom = where == corner_bottom_left || where == corner_bottom_right;

    // Candidates are the corner itself and every spot flush against an obstacle on the corner's side: for a
    // top-left corner, just right of each box and just below it. One of them is where a box that cannot sit
    // in the corner first fits, and trying their combinations covers sitting beside one box and under another.
    constexpr std::size_t k_axis = 1 + k_chrome + 64;
    std::array<float, k_axis> xs {};
    std::array<float, k_axis> ys {};
    std::size_t nx = 0;
    std::size_t ny = 0;

    const auto push = [&](const rect& box) {
        if(nx < k_axis) {
            xs[nx++] = right ? box.x0 - margin - width : box.x1 + margin;
        }
        if(ny < k_axis) {
            ys[ny++] = bottom ? box.y0 - margin - height : box.y1 + margin;
        }
    };

    xs[nx++] = right ? g_screen.x1 - margin - width : g_screen.x0 + margin;
    ys[ny++] = bottom ? g_screen.y1 - margin - height : g_screen.y0 + margin;
    for(std::size_t i = 0; i < g_chrome_count; ++i) {
        if(g_chrome[i].by != chrome_menu) {
            push(g_chrome[i].box);
        }
    }
    for(const int id : g_drawn) {
        const space& set = g_spaces[static_cast<std::size_t>(id)];
        for(int i = 0; i < set.count; ++i) {
            push(set.boxes[static_cast<std::size_t>(i)]);
        }
    }

    const float corner_x = right ? g_screen.x1 : g_screen.x0;
    const float corner_y = bottom ? g_screen.y1 : g_screen.y0;
    // Half a pixel of slack at the screen's edges, for candidates computed from them.
    constexpr float k_slack = 0.5f;

    bool found = false;
    float best_distance = std::numeric_limits<float>::max();
    rect best {};

    for(std::size_t ix = 0; ix < nx; ++ix) {
        for(std::size_t iy = 0; iy < ny; ++iy) {
            const rect box { xs[ix], ys[iy], xs[ix] + width, ys[iy] + height };
            if(box.x0 < g_screen.x0 + margin - k_slack || box.x1 > g_screen.x1 - margin + k_slack || box.y0 < g_screen.y0 + margin - k_slack
                || box.y1 > g_screen.y1 - margin + k_slack) {
                continue;
            }

            // Measured from the screen corner to the box's own corner on that side.
            const float dx = (right ? box.x1 : box.x0) - corner_x;
            const float dy = (bottom ? box.y1 : box.y0) - corner_y;
            const float distance = dx * dx + dy * dy;
            if(distance >= best_distance || blocked(box, margin)) {
                continue;
            }

            found = true;
            best_distance = distance;
            best = box;
        }
    }

    if(!found) {
        return false;
    }

    note(owner, best);
    out[0] = best.x0;
    out[1] = best.y0;
    return true;
}
} // namespace tw::lua::hud_space
