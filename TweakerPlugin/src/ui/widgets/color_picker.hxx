#pragma once

#include <imgui.h>

#include <string>

namespace tw::ui::widgets
{
class color_picker {
public:
    explicit color_picker(const char* id, ImVec2 size = { 0.f, 0.f });

    void set_size(ImVec2 size) noexcept;
    void set_color(const ImVec4& rgba) noexcept;

    void update();
    void update(ImVec4& color);

    [[nodiscard]] ImVec4 color() const noexcept;
    [[nodiscard]] bool changed() const noexcept;

private:
    void update_internal();
    void rebuild_rgba_from_hsv() noexcept;
    void sync_fields_from_rgba() noexcept;
    void apply_rgba_ints() noexcept;
    bool try_apply_hex() noexcept;

    std::string m_id;
    ImVec2 m_size;

    float m_h = 0.5f;
    float m_s = 0.75f;
    float m_v = 0.9f;
    float m_a = 1.f;
    ImVec4 m_rgba { 0.2f, 0.8f, 0.75f, 1.f };

    bool m_changed = false;

    char m_hex_buf[16] = "#FF33CCBF";
    int m_rgba_i[4] = { 51, 204, 191, 255 };
};
} // namespace tw::ui::widgets
