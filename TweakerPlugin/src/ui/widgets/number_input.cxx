#include "ui/widgets/number_input.hxx"

#include "ui/theme.hxx"

#include <algorithm>

namespace
{
constexpr float k_rounding = 6.f;
constexpr int k_step = 1;
constexpr int k_step_fast = 5;
} // namespace

namespace tw::ui::widgets
{
number_input::number_input(const char* id, float width) : m_id(id ? id : "number_input"), m_width(width)
{
}

void number_input::set_width(float width) noexcept
{
    m_width = width;
}

void number_input::set_range(int min_value, int max_value) noexcept
{
    m_min = min_value;
    m_max = (std::max)(min_value, max_value);
    clamp_value();
}

void number_input::set_value(int value) noexcept
{
    const int clamped = std::clamp(value, m_min, m_max);
    if(clamped == m_value) {
        return;
    }

    m_value = clamped;
}

void number_input::clamp_value() noexcept
{
    m_value = std::clamp(m_value, m_min, m_max);
}

void number_input::update()
{
    m_changed = false;

    ImGui::PushID(m_id.c_str());

    // The field's own chrome, plus the two stepper buttons InputScalar draws when given a step -
    // those are stock ImGui buttons, so they need the button colours and the frame rounding pushed
    // too or they come out square and grey next to everything else.
    ImGui::PushStyleColor(ImGuiCol_FrameBg, theme::surface_input);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, theme::surface_hover);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, theme::surface_elevated);
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_primary);
    ImGui::PushStyleColor(ImGuiCol_Border, theme::border);
    ImGui::PushStyleColor(ImGuiCol_Button, theme::surface_soft);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::accent_hover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, theme::accent_pressed);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, k_rounding);

    ImGui::SetNextItemWidth(m_width);

    int edited = m_value;
    const bool stepped = ImGui::InputScalar("##value", ImGuiDataType_S32, &edited, &k_step, &k_step_fast, "%d");

    // Two commit points, and both are needed: InputScalar returns true immediately for a stepper
    // click but only reports typed text once the field is left, which IsItemDeactivatedAfterEdit
    // catches. Reporting on every keystroke instead would put a wire message behind each digit.
    const bool committed = ImGui::IsItemDeactivatedAfterEdit();

    if(stepped || committed) {
        const int clamped = std::clamp(edited, m_min, m_max);
        if(clamped != m_value) {
            m_value = clamped;
            m_changed = true;
        }
        // No else: typing something out of range that clamps back to the value already held leaves
        // nothing to publish and nothing to write (the assignment that used to sit here could only
        // ever store m_value into itself). The field redraws itself regardless - `edited` is
        // re-seeded from m_value at the top of the next frame.
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(8);
    ImGui::PopID();
}

int number_input::value() const noexcept
{
    return m_value;
}

bool number_input::changed() const noexcept
{
    return m_changed;
}
} // namespace tw::ui::widgets
