#pragma once

#include <imgui.h>

#include <libtweeny/tweeny.hxx>

#include <string>

namespace tw::ui::widgets
{
class toggle {
public:
    explicit toggle(const char* id, ImVec2 size = { 40.f, 22.f });

    void set_size(ImVec2 size) noexcept;
    void set_checked(bool checked) noexcept;

    void update();

    [[nodiscard]] bool checked() const noexcept;
    [[nodiscard]] bool changed() const noexcept;
    [[nodiscard]] bool hovered() const noexcept;

private:
    void retarget(float target);

    std::string m_id;
    ImVec2 m_size;

    bool m_checked = false;
    bool m_changed = false;
    bool m_hovered = false;
    bool m_held = false;

    float m_thumb_t = 0.f;
    tweeny::tween<float> m_thumb_tween {};
};
} // namespace tw::ui::widgets
