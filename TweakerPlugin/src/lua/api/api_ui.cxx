#include "pch.hxx"

#include "lua/api/api_ui.hxx"

#include "lua/lua_tabs.hxx"

namespace
{
std::string_view text_of(const char* text) noexcept
{
    return text != nullptr ? std::string_view { text } : std::string_view {};
}

// The prelude's string arrays as views. Cold: options and rows are set when a script declares or changes them.
std::vector<std::string_view> views_of(const char* const* texts, int count)
{
    std::vector<std::string_view> views;
    if(count <= 0) {
        return views;
    }

    views.reserve(static_cast<std::size_t>(count));
    for(int i = 0; i < count; ++i) {
        views.push_back(texts != nullptr ? text_of(texts[i]) : std::string_view {});
    }
    return views;
}
} // namespace

namespace tw::lua::api
{
extern "C" {
int tw_ui_tab(int owner, const char* label) noexcept
{
    return tw::lua::tabs::tab(owner, text_of(label));
}

int tw_ui_add(int parent, int kind, const char* label) noexcept
{
    return tw::lua::tabs::add(parent, kind, text_of(label));
}

void tw_ui_range(int node, double min_value, double max_value) noexcept
{
    tw::lua::tabs::set_range(node, min_value, max_value);
}

void tw_ui_options(int node, const char* const* labels, int count) noexcept
{
    const std::vector<std::string_view> views = views_of(labels, count);
    tw::lua::tabs::set_options(node, views);
}

void tw_ui_items(int node, const char* const* text, const char* const* subtext, int count) noexcept
{
    const std::vector<std::string_view> texts = views_of(text, count);
    const std::vector<std::string_view> subtexts = views_of(subtext, subtext != nullptr ? count : 0);
    tw::lua::tabs::set_items(node, texts, subtexts);
}

void tw_ui_height(int node, float height) noexcept
{
    tw::lua::tabs::set_height(node, height);
}

double tw_ui_get(int node) noexcept
{
    return tw::lua::tabs::value(node);
}

void tw_ui_set(int node, double value) noexcept
{
    tw::lua::tabs::set_value(node, value);
}

void tw_ui_text(int node, int which, const char* text) noexcept
{
    if(which != static_cast<int>(tw::lua::tabs::text_slot::label) && which != static_cast<int>(tw::lua::tabs::text_slot::tooltip)) {
        return;
    }
    tw::lua::tabs::set_text(node, static_cast<tw::lua::tabs::text_slot>(which), text_of(text));
}

void tw_ui_flags(int node, int enabled, int visible) noexcept
{
    tw::lua::tabs::set_flags(node, enabled, visible);
}

int tw_ui_tab_visible(int node) noexcept
{
    return tw::lua::tabs::tab_visible(node) ? 1 : 0;
}

int tw_ui_next_event(int* node, double* value, int* committed) noexcept
{
    int handle = -1;
    double v = 0.0;
    bool done = false;
    if(node == nullptr || value == nullptr || committed == nullptr || !tw::lua::tabs::next_event(handle, v, done)) {
        return 0;
    }

    *node = handle;
    *value = v;
    *committed = done ? 1 : 0;
    return 1;
}
}
} // namespace tw::lua::api
