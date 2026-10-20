#include "pch.hxx"

#include "lua/lua_tabs.hxx"

#include "lua/lua_diag.hxx"
#include "lua/lua_registry.hxx"

#include "ui/form/form.hxx"
#include "ui/plugins/interactive/menu.hxx"

namespace
{
namespace form = tw::ui::form;
namespace menu = tw::ui::plugins::interactive::menu;

// A handle is the slot's index in the low bits and its generation above them. 2^20 slots is far past anything a
// session gets to (k_max_nodes per script, reused once freed); 11 bits of generation keep the int positive.
constexpr int k_index_bits = 20;
constexpr std::uint32_t k_index_mask = (1u << k_index_bits) - 1u;
constexpr std::uint32_t k_generation_mask = (1u << 11) - 1u;

// One handle's worth of state: what it points at, and the event waiting for the script, if any.
struct slot {
    std::uint32_t generation = 1;
    int owner = -1;
    int tab = -1;  // index into g_tabs
    int node = -1; // in that tab's form; -1 is the tab itself
    bool live = false;

    bool pending = false; // in g_queue
    bool committed = false;
    double value = 0.0;
};

struct tab_record {
    int owner = -1;
    int ordinal = 0; // among its script's tabs, in declaration order: what the key is made of
    int handle = -1; // its own slot - also the token the menu hands back to draw_tab
    std::string label;
    std::string key;
    std::unique_ptr<form::form> page;
    std::vector<int> node_slots; // slot index by form node
    std::uint32_t drawn = 0;     // g_serial of the frame it was last drawn in
    bool dead = false;
};

std::vector<slot> g_slots;
std::vector<int> g_free_slots;

// Null entries are free and reused: a slot points at its tab by index, so the vector never shrinks.
std::vector<std::unique_ptr<tab_record>> g_tabs;

// Per script, by id: nodes declared (its tabs included), and how many tabs it has declared this run.
std::vector<int> g_owner_nodes;
std::vector<int> g_owner_tabs;

// The events, as slot indices; g_read is how far the drain has got. Reserved to the slot count whenever a slot is
// made, so pushing one in a frame never allocates: at most one entry per slot is ever in it.
std::vector<int> g_queue;
std::size_t g_read = 0;

// The menu's list is stale: a tab was declared, dropped or relabelled. The registry's revision covers the rest.
bool g_dirty = false;
std::uint32_t g_seen_revision = 0;
bool g_published = false;

bool g_any_dead = false;

// Counts overlay frames, from end_frame(). Starts past zero so that a tab never drawn is never "drawn last frame".
std::uint32_t g_serial = 2;

[[nodiscard]] int handle_of(int index) noexcept
{
    return static_cast<int>((g_slots[static_cast<std::size_t>(index)].generation << k_index_bits) | static_cast<std::uint32_t>(index));
}

// The live slot a handle names, or null - for a stale generation, an index out of range, a dropped script.
[[nodiscard]] slot* resolve(int handle) noexcept
{
    if(handle < 0) {
        return nullptr;
    }

    const std::uint32_t bits = static_cast<std::uint32_t>(handle);
    const std::uint32_t index = bits & k_index_mask;
    if(index >= g_slots.size()) {
        return nullptr;
    }

    slot& s = g_slots[index];
    if(!s.live || s.generation != (bits >> k_index_bits)) {
        return nullptr;
    }

    return &s;
}

[[nodiscard]] tab_record* tab_of(const slot& s) noexcept
{
    if(s.tab < 0 || s.tab >= static_cast<int>(g_tabs.size())) {
        return nullptr;
    }

    tab_record* const record = g_tabs[static_cast<std::size_t>(s.tab)].get();
    return record != nullptr && !record->dead ? record : nullptr;
}

// A slot for a new node or tab. Reuses a freed one - its generation already moved on when it was freed.
[[nodiscard]] int make_slot(int owner, int tab, int node)
{
    int index = -1;
    if(!g_free_slots.empty()) {
        index = g_free_slots.back();
        g_free_slots.pop_back();
    }
    else {
        index = static_cast<int>(g_slots.size());
        g_slots.emplace_back();
        g_queue.reserve(g_slots.size());
    }

    slot& s = g_slots[static_cast<std::size_t>(index)];
    s.owner = owner;
    s.tab = tab;
    s.node = node;
    s.live = true;
    s.pending = false;
    s.committed = false;
    s.value = 0.0;
    return index;
}

void free_slot(int index) noexcept
{
    slot& s = g_slots[static_cast<std::size_t>(index)];
    s.live = false;
    s.pending = false;
    s.generation = (s.generation + 1) & k_generation_mask;
    if(s.generation == 0) {
        s.generation = 1;
    }
    g_free_slots.push_back(index);
}

int& owner_counter(std::vector<int>& counters, int owner)
{
    if(owner >= static_cast<int>(counters.size())) {
        counters.resize(static_cast<std::size_t>(owner) + 1, 0);
    }
    return counters[static_cast<std::size_t>(owner)];
}

// Cut to `limit` bytes without splitting a UTF-8 sequence, and said once per place: a label that long is a
// mistake worth knowing about, and not one worth stopping the script for.
std::string_view fit(int owner, std::string_view text, std::size_t limit, const char* what) noexcept
{
    if(text.size() <= limit) {
        return text;
    }

    std::size_t cut = limit;
    while(cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0u) == 0x80u) {
        --cut;
    }

    tw::lua::diag::report(owner, tw::lua::diag::level::warn, "tw.ui", std::format("{} is {} bytes - cut to {}", what, text.size(), limit));
    return text.substr(0, cut);
}

[[nodiscard]] char fold(char c) noexcept
{
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

[[nodiscard]] bool same_label(std::string_view a, std::string_view b) noexcept
{
    if(a.size() != b.size()) {
        return false;
    }
    for(std::size_t i = 0; i < a.size(); ++i) {
        if(fold(a[i]) != fold(b[i])) {
            return false;
        }
    }
    return true;
}

// What the script calls itself: its @name, or its file without .lua.
std::string script_name(int owner)
{
    const tw::lua::script* const script = tw::lua::registry::find(owner);
    if(script == nullptr) {
        return {};
    }
    if(!script->name.empty()) {
        return script->name;
    }

    std::string_view file = script->file;
    if(file.size() > 4 && same_label(file.substr(file.size() - 4), ".lua")) {
        file.remove_suffix(4);
    }
    return std::string { file };
}

[[nodiscard]] bool suspended(int owner) noexcept
{
    const tw::lua::script* const script = tw::lua::registry::find(owner);
    return script != nullptr && script->health.suspended;
}

// The menu's draw callback: the page, from the model, without a single entry into Lua.
float draw_tab(int token) noexcept
{
    const slot* const s = resolve(token);
    tab_record* const record = s != nullptr && s->node < 0 ? tab_of(*s) : nullptr;
    if(record == nullptr) {
        return 0.f;
    }

    record->drawn = g_serial;

    // A suspended script's page is switched off as a whole (§2.5): the values stay readable, nothing takes input.
    const bool off = suspended(record->owner);
    if(off) {
        form::begin_disabled();
    }
    record->page->draw();
    if(off) {
        form::end_disabled();
    }

    for(const form::event& e : record->page->events()) {
        if(e.node < 0 || e.node >= static_cast<int>(record->node_slots.size())) {
            continue;
        }

        const int index = record->node_slots[static_cast<std::size_t>(e.node)];
        slot& target = g_slots[static_cast<std::size_t>(index)];
        if(!target.pending) {
            target.pending = true;
            target.committed = false;
            g_queue.push_back(index);
        }
        target.value = e.value;
        target.committed = target.committed || e.committed;
    }

    return record->page->min_width();
}

// The menu's list, from scratch: the run order, each script's live tabs in declaration order, clashing labels
// with the script's name in front.
void publish()
{
    struct entry {
        const tab_record* record;
        std::string label;
    };

    std::vector<entry> list;
    for(const int owner : tw::lua::registry::order()) {
        const std::size_t first = list.size();
        for(const std::unique_ptr<tab_record>& record : g_tabs) {
            if(record != nullptr && !record->dead && record->owner == owner) {
                list.push_back({ record.get(), {} });
            }
        }
        std::sort(list.begin() + static_cast<std::ptrdiff_t>(first), list.end(), [](const entry& a, const entry& b) {
            return a.record->ordinal < b.record->ordinal;
        });
    }

    // A tab with no label is named after its script, which is also what keeps it from being a blank button.
    for(entry& e : list) {
        e.label = e.record->label.empty() ? script_name(e.record->owner) : e.record->label;
    }

    const std::vector<std::string> fixed = menu::fixed_tab_labels();
    std::vector<std::string> shown(list.size());
    for(std::size_t i = 0; i < list.size(); ++i) {
        bool clash = std::ranges::any_of(fixed, [&](const std::string& label) { return same_label(label, list[i].label); });
        for(std::size_t j = 0; !clash && j < list.size(); ++j) {
            clash = j != i && same_label(list[j].label, list[i].label);
        }

        shown[i] = clash ? std::format("{} - {}", script_name(list[i].record->owner), list[i].label) : list[i].label;
    }

    std::vector<menu::dynamic_tab> tabs;
    tabs.reserve(list.size());
    for(std::size_t i = 0; i < list.size(); ++i) {
        const tab_record& record = *list[i].record;
        tabs.push_back({ record.key, shown[i], &draw_tab, record.handle, !suspended(record.owner) });
    }

    menu::set_dynamic_tabs(tabs);
    g_published = true;
}

void collect() noexcept
{
    for(std::size_t i = 0; i < g_tabs.size(); ++i) {
        tab_record* const record = g_tabs[i].get();
        if(record == nullptr || !record->dead) {
            continue;
        }

        for(const int index : record->node_slots) {
            if(index >= 0) {
                free_slot(index);
            }
        }
        free_slot(static_cast<int>(static_cast<std::uint32_t>(record->handle) & k_index_mask));
        g_tabs[i].reset();
    }

    // Entries of what was just freed would point at slots about to be somebody else's.
    std::erase_if(g_queue, [](int index) { return !g_slots[static_cast<std::size_t>(index)].live; });
    g_read = std::min(g_read, g_queue.size());
    g_any_dead = false;
}
} // namespace

namespace tw::lua::tabs
{
int tab(int owner, std::string_view label) noexcept
{
    const tw::lua::script* const script = tw::lua::registry::find(owner);
    if(script == nullptr) {
        return -1;
    }

    int& nodes = owner_counter(g_owner_nodes, owner);
    if(nodes >= k_max_nodes) {
        tw::lua::diag::report(owner, tw::lua::diag::level::error, "tw.ui", std::format("more than {} controls - tw.ui.tab ignored", k_max_nodes));
        return -1;
    }

    int& ordinal = owner_counter(g_owner_tabs, owner);

    auto record = std::make_unique<tab_record>();
    record->owner = owner;
    record->ordinal = ordinal;
    record->label.assign(fit(owner, label, k_max_label, "a tab label"));
    record->key = std::format("{}#{}", script->file, ordinal);
    record->page = std::make_unique<form::form>(std::format("s{}t{}", owner, ordinal));

    int index = -1;
    for(std::size_t i = 0; i < g_tabs.size(); ++i) {
        if(g_tabs[i] == nullptr) {
            index = static_cast<int>(i);
            break;
        }
    }
    if(index < 0) {
        index = static_cast<int>(g_tabs.size());
        g_tabs.emplace_back();
    }

    const int at = make_slot(owner, index, -1);
    record->handle = handle_of(at);
    g_tabs[static_cast<std::size_t>(index)] = std::move(record);

    ++ordinal;
    ++nodes;
    g_dirty = true;
    return handle_of(at);
}

int add(int parent, int kind, std::string_view label) noexcept
{
    const slot* const p = resolve(parent);
    tab_record* const record = p != nullptr ? tab_of(*p) : nullptr;
    if(record == nullptr) {
        return -1;
    }

    const int owner = p->owner;
    if(kind < 0 || kind >= form::k_kind_count) {
        tw::lua::diag::report(owner, tw::lua::diag::level::error, "tw.ui", std::format("no control of kind {}", kind));
        return -1;
    }

    // A tab is the top level; anything else must be a group, and the form says whether it is one.
    if(p->node >= 0 && record->page->kind_of(p->node) != form::kind::group) {
        tw::lua::diag::report(owner, tw::lua::diag::level::error, "tw.ui", "controls go on a tab or into a group");
        return -1;
    }

    int& nodes = owner_counter(g_owner_nodes, owner);
    if(nodes >= k_max_nodes) {
        tw::lua::diag::report(owner, tw::lua::diag::level::error, "tw.ui", std::format("more than {} controls - the rest are ignored", k_max_nodes));
        return -1;
    }

    const std::size_t limit = static_cast<form::kind>(kind) == form::kind::text ? k_max_text : k_max_label;
    const int node = record->page->add(static_cast<form::kind>(kind), p->node, fit(owner, label, limit, "a label"));
    if(node < 0) {
        tw::lua::diag::report(owner, tw::lua::diag::level::error, "tw.ui",
            std::format("groups nest {} deep at most - this one is ignored, and what goes in it", form::k_max_group_depth));
        return -1;
    }

    const int tab_index = p->tab;
    const int at = make_slot(owner, tab_index, node);
    if(node >= static_cast<int>(record->node_slots.size())) {
        record->node_slots.resize(static_cast<std::size_t>(node) + 1, -1);
    }
    record->node_slots[static_cast<std::size_t>(node)] = at;

    ++nodes;
    return handle_of(at);
}

void set_range(int handle, double min_value, double max_value) noexcept
{
    const slot* const s = resolve(handle);
    tab_record* const record = s != nullptr && s->node >= 0 ? tab_of(*s) : nullptr;
    if(record != nullptr) {
        record->page->set_range(s->node, min_value, max_value);
    }
}

void set_options(int handle, std::span<const std::string_view> labels) noexcept
{
    const slot* const s = resolve(handle);
    tab_record* const record = s != nullptr && s->node >= 0 ? tab_of(*s) : nullptr;
    if(record == nullptr) {
        return;
    }

    if(labels.size() > static_cast<std::size_t>(k_max_options)) {
        tw::lua::diag::report(s->owner, tw::lua::diag::level::warn, "tw.ui",
            std::format("{} options - a choice shows the first {}", labels.size(), k_max_options));
        labels = labels.first(static_cast<std::size_t>(k_max_options));
    }

    std::vector<std::string_view> fitted;
    fitted.reserve(labels.size());
    for(const std::string_view label : labels) {
        fitted.push_back(fit(s->owner, label, k_max_label, "an option"));
    }
    record->page->set_options(s->node, fitted);
}

void set_items(int handle, std::span<const std::string_view> text, std::span<const std::string_view> subtext) noexcept
{
    const slot* const s = resolve(handle);
    tab_record* const record = s != nullptr && s->node >= 0 ? tab_of(*s) : nullptr;
    if(record == nullptr) {
        return;
    }

    std::size_t count = text.size();
    if(count > static_cast<std::size_t>(k_max_rows)) {
        tw::lua::diag::report(s->owner, tw::lua::diag::level::warn, "tw.ui", std::format("{} rows - a list shows the first {}", count, k_max_rows));
        count = static_cast<std::size_t>(k_max_rows);
    }

    std::vector<tw::ui::widgets::list_item_content> items(count);
    for(std::size_t i = 0; i < count; ++i) {
        items[i].text.assign(fit(s->owner, text[i], k_max_label, "a list row"));
        if(i < subtext.size()) {
            items[i].subtext.assign(fit(s->owner, subtext[i], k_max_label, "a list row"));
        }
    }
    record->page->set_items(s->node, items);
}

void set_height(int handle, float height) noexcept
{
    const slot* const s = resolve(handle);
    tab_record* const record = s != nullptr && s->node >= 0 ? tab_of(*s) : nullptr;
    if(record != nullptr) {
        record->page->set_height(s->node, height);
    }
}

double value(int handle) noexcept
{
    const slot* const s = resolve(handle);
    const tab_record* const record = s != nullptr && s->node >= 0 ? tab_of(*s) : nullptr;
    if(record == nullptr) [[unlikely]] {
        return std::numeric_limits<double>::quiet_NaN();
    }

    return record->page->value(s->node);
}

void set_value(int handle, double value) noexcept
{
    const slot* const s = resolve(handle);
    tab_record* const record = s != nullptr && s->node >= 0 ? tab_of(*s) : nullptr;
    if(record != nullptr) {
        record->page->set_value(s->node, value);
    }
}

void set_text(int handle, text_slot which, std::string_view text) noexcept
{
    const slot* const s = resolve(handle);
    tab_record* const record = s != nullptr ? tab_of(*s) : nullptr;
    if(record == nullptr) {
        return;
    }

    // The tab's own label is the menu's, not the form's: it goes out with the next list.
    if(s->node < 0) {
        if(which == text_slot::label) {
            const std::string_view label = fit(s->owner, text, k_max_label, "a tab label");
            if(label != record->label) {
                record->label.assign(label);
                g_dirty = true;
            }
        }
        return;
    }

    if(which == text_slot::tooltip) {
        record->page->set_tooltip(s->node, fit(s->owner, text, k_max_text, "a tooltip"));
        return;
    }

    const std::size_t limit = record->page->kind_of(s->node) == form::kind::text ? k_max_text : k_max_label;
    record->page->set_label(s->node, fit(s->owner, text, limit, "a label"));
}

void set_flags(int handle, int enabled, int visible) noexcept
{
    const slot* const s = resolve(handle);
    tab_record* const record = s != nullptr && s->node >= 0 ? tab_of(*s) : nullptr;
    if(record == nullptr) {
        return;
    }

    if(enabled == 0 || enabled == 1) {
        record->page->set_enabled(s->node, enabled == 1);
    }
    if(visible == 0 || visible == 1) {
        record->page->set_visible(s->node, visible == 1);
    }
}

bool tab_visible(int handle) noexcept
{
    const slot* const s = resolve(handle);
    const tab_record* const record = s != nullptr ? tab_of(*s) : nullptr;
    return record != nullptr && (record->drawn == g_serial || record->drawn + 1 == g_serial);
}

bool has_events() noexcept
{
    return g_read < g_queue.size();
}

bool next_event(int& handle, double& value, bool& committed) noexcept
{
    while(g_read < g_queue.size()) {
        const int index = g_queue[g_read++];
        slot& s = g_slots[static_cast<std::size_t>(index)];
        const bool was_pending = s.pending;
        s.pending = false;

        // A suspended script's page is switched off; what is still queued for it came from the frame it was
        // suspended in, and it would run the moment it is resumed, long after the fact.
        if(!was_pending || !s.live || suspended(s.owner)) {
            continue;
        }

        handle = handle_of(index);
        value = s.value;
        committed = s.committed;
        return true;
    }

    g_queue.clear();
    g_read = 0;
    return false;
}

void drop_owner(int owner) noexcept
{
    bool any = false;
    for(const std::unique_ptr<tab_record>& record : g_tabs) {
        if(record == nullptr || record->dead || record->owner != owner) {
            continue;
        }

        record->dead = true;
        any = true;
        g_slots[static_cast<std::size_t>(static_cast<std::uint32_t>(record->handle) & k_index_mask)].live = false;
        for(const int index : record->node_slots) {
            if(index >= 0) {
                g_slots[static_cast<std::size_t>(index)].live = false;
            }
        }
    }

    // Counted afresh by the next run: a reloaded script's tabs get the same ordinals, so the same keys.
    if(owner >= 0 && owner < static_cast<int>(g_owner_nodes.size())) {
        g_owner_nodes[static_cast<std::size_t>(owner)] = 0;
    }
    if(owner >= 0 && owner < static_cast<int>(g_owner_tabs.size())) {
        g_owner_tabs[static_cast<std::size_t>(owner)] = 0;
    }

    if(any) {
        g_any_dead = true;
        g_dirty = true;
    }
}

void begin_frame() noexcept
{
    const std::uint32_t revision = tw::lua::registry::revision();
    if(!g_dirty && revision == g_seen_revision) [[likely]] {
        return;
    }

    g_dirty = false;
    g_seen_revision = revision;

    // A session with no script tabs, ever, never touches the menu.
    if(!g_published && std::ranges::none_of(g_tabs, [](const std::unique_ptr<tab_record>& record) { return record != nullptr; })) {
        return;
    }

    publish();
}

void end_frame() noexcept
{
    ++g_serial;
    if(g_any_dead) [[unlikely]] {
        collect();
    }
}

void clear() noexcept
{
    if(g_published) {
        menu::set_dynamic_tabs({});
    }

    g_slots.clear();
    g_free_slots.clear();
    g_tabs.clear();
    g_owner_nodes.clear();
    g_owner_tabs.clear();
    g_queue.clear();
    g_read = 0;
    g_dirty = false;
    g_seen_revision = 0;
    g_published = false;
    g_any_dead = false;
}
} // namespace tw::lua::tabs
