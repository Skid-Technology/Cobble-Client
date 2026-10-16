#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "theme.hpp"
#include "widgets.hpp"

namespace ui {
namespace theme {

ImU32 getAccent(float alpha) {
    ImVec4 col = ImVec4(accentColor[0], accentColor[1], accentColor[2], accentColor[3]);
    col.w *= alpha * ImGui::GetStyle().Alpha;
    return ImGui::ColorConvertFloat4ToU32(col);
}

void apply() {
    ImGui::StyleColorsDark();
    auto& style = ImGui::GetStyle();

    style.WindowRounding = s(rounding::Window);
    style.ChildRounding = s(rounding::Child);
    style.FrameRounding = s(rounding::Frame);
    style.GrabRounding = s(rounding::Grab);
    style.PopupRounding = s(rounding::Popup);
    style.ScrollbarRounding = s(rounding::Scrollbar);

    style.WindowPadding = {0, 0};
    style.FramePadding = {s(8), s(6)};
    style.ItemSpacing = {s(8), s(8)};
    style.ItemInnerSpacing = {s(8), s(4)};

    ImVec4* colors = style.Colors;

    colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.063f, 0.063f, 0.063f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.078f, 0.078f, 0.078f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.122f, 0.122f, 0.122f, 1.00f);
    colors[ImGuiCol_Border] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    colors[ImGuiCol_BorderShadow] = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);

    colors[ImGuiCol_FrameBg] = ImVec4(0.063f, 0.063f, 0.063f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.082f, 0.082f, 0.082f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.102f, 0.102f, 0.102f, 1.00f);

    colors[ImGuiCol_Header] = ImVec4(0.102f, 0.102f, 0.102f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.122f, 0.122f, 0.122f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.141f, 0.141f, 0.141f, 1.00f);

    colors[ImGuiCol_Button] = ImVec4(0.102f, 0.102f, 0.102f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.122f, 0.122f, 0.122f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.141f, 0.141f, 0.141f, 1.00f);

    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.110f, 0.110f, 0.110f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.129f, 0.129f, 0.129f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.149f, 0.149f, 0.149f, 1.00f);

    colors[ImGuiCol_CheckMark] = ImVec4(accentColor[0], accentColor[1], accentColor[2], 1.0f);
    colors[ImGuiCol_SliderGrab] = ImVec4(accentColor[0], accentColor[1], accentColor[2], 1.0f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(accentColor[0], accentColor[1], accentColor[2], 1.0f);
}

}  
}  
