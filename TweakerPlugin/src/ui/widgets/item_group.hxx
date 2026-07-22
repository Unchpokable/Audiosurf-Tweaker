#pragma once

#include <imgui.h>

#include <string>
#include <string_view>

namespace tw::ui::widgets
{
class item_group {
public:
    explicit item_group(const char* id, const char* group_title = "");

    void set_group_title(std::string_view title);
    void set_rounding(float rounding) noexcept;
    void set_outer_padding(ImVec2 padding) noexcept;
    void set_inner_padding(ImVec2 padding) noexcept;

    void begin();
    void end();

private:
    std::string m_id;
    std::string m_group_title;
    float m_rounding = 5.f;
    ImVec2 m_outer_padding { 0.f, 4.f };
    ImVec2 m_inner_padding { 8.f, 8.f };

    bool m_active = false;
    ImVec2 m_origin {};
    float m_title_h = 0.f;
};
} // namespace tw::ui::widgets
