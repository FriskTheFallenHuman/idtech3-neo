// UI style adapted from the supplied reference editor PreferencePanel.cpp.
#include "NextUI.h"
namespace de {
void applyDesktopStyle(float scale) {
    ImGui::StyleColorsLight();
    auto &s = ImGui::GetStyle();
    s.WindowRounding = s.ChildRounding = s.FrameRounding = s.PopupRounding = s.GrabRounding =
        s.ScrollbarRounding = 0;
    s.WindowPadding = {7, 6};
    s.FramePadding = {3, 2};
    s.ItemSpacing = {5, 4};
    s.ItemInnerSpacing = {3, 3};
    s.ScrollbarSize = 14;
    s.WindowBorderSize = 1;
    s.FrameBorderSize = 1;
    s.WindowTitleAlign = {.5f, .5f};
    s.WindowMenuButtonPosition = ImGuiDir_None;
    auto &c = s.Colors;
    c[ImGuiCol_Text] = {0, 0, 0, 1};
    c[ImGuiCol_WindowBg] = {.67f, .67f, .67f, 1};
    c[ImGuiCol_ChildBg] = {.67f, .67f, .67f, 1};
    c[ImGuiCol_PopupBg] = {.72f, .72f, .72f, 1};
    c[ImGuiCol_Border] = {.16f, .16f, .16f, 1};
    c[ImGuiCol_TitleBg] = c[ImGuiCol_TitleBgActive] = {.1f, .1f, .1f, 1};
    c[ImGuiCol_TitleBgCollapsed] = {.3f, .3f, .3f, 1};
    c[ImGuiCol_FrameBg] = {.94f, .94f, .94f, 1};
    c[ImGuiCol_FrameBgHovered] = {1, 1, 1, 1};
    c[ImGuiCol_FrameBgActive] = {1, 1, 1, 1};
    c[ImGuiCol_Button] = {.8f, .8f, .8f, 1};
    c[ImGuiCol_ButtonHovered] = {.9f, .9f, .9f, 1};
    c[ImGuiCol_ButtonActive] = {.55f, .55f, .55f, 1};
    c[ImGuiCol_CheckMark] = {0, 0, 0, 1};
    c[ImGuiCol_Header] = {.93f, .93f, .93f, 1};
    c[ImGuiCol_HeaderHovered] = {.82f, .82f, .82f, 1};
    c[ImGuiCol_HeaderActive] = {1, 1, 1, 1};
    c[ImGuiCol_SliderGrab] = {.3f, .3f, .3f, 1};
    c[ImGuiCol_ResizeGrip] = {.3f, .3f, .3f, 1};
    s.ScaleAllSizes(scale);
}
} // namespace de
