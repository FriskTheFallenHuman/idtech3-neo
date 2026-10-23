#include "MapWindow.h"
#include <algorithm>
namespace de {
void drawMapWindow(App &a) {
    auto vp = ImGui::GetMainViewport();
    float bar = ImGui::GetFrameHeight();
    ImGui::SetNextWindowPos({0, bar});
    ImGui::SetNextWindowSize({vp->Size.x, vp->Size.y - bar});
    ImGui::Begin("DoomEd workspace", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
    drawToolbar(a);
    drawReferenceImageControls(a);
    float side = std::min(330 * a.dpi, ImGui::GetContentRegionAvail().x * .4f);
    ImGui::BeginChild("Map",
                      {ImGui::GetContentRegionAvail().x - side - ImGui::GetStyle().ItemSpacing.x,
                       -ImGui::GetFrameHeightWithSpacing()},
                      ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    a.view.origin = ImGui::GetCursorScreenPos();
    a.view.size = ImGui::GetContentRegionAvail();
    a.view.size.x = std::max(a.view.size.x, 1.f);
    a.view.size.y = std::max(a.view.size.y, 1.f);
    ImGui::InvisibleButton("Canvas", a.view.size,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
                               ImGuiButtonFlags_MouseButtonMiddle);
    mapRespond(a);
    drawMap(a, ImGui::GetWindowDrawList());
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("Inspector", {side, -ImGui::GetFrameHeightWithSpacing()},
                      ImGuiChildFlags_Borders);
    ImGui::PushItemWidth(120 * a.dpi);
    ImGui::Text("%s%s", a.mapName.c_str(), a.world.dirty() ? " *" : "");
    ImGui::Separator();
    if (a.view.mode == 2)
        drawThingPanel(a);
    else {
        drawLinePanel(a);
        ImGui::Separator();
        drawSectorEditor(a);
    }
    ImGui::PopItemWidth();
    ImGui::EndChild();
    ImGui::Text("%s  |  %.0f, %.0f  |  %.0f%%", a.status.c_str(), a.view.mouse.x, a.view.mouse.y,
                a.view.zoom * 100);
    ImGui::End();
}
} // namespace de
