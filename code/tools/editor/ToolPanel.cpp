#include "ToolPanel.h"
namespace de {
void drawToolbar(App &a) {
    ImGui::SetNextItemWidth(100 * a.dpi);
    if (ImGui::Combo("##mode", &a.view.mode, "Vertices\0Walls\0Things\0Sectors\0")) {
        a.world.deselect();
        a.pickedSectorSides.clear();
        a.view.tool = 0;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(145 * a.dpi);
    if (ImGui::Combo("##tool", &a.view.tool,
                     "Select / move\0Draw sector\0Rectangle sector\0Place thing\0"))
        a.view.drawing = false;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(70 * a.dpi);
    int grid = 0;
    int values[] = {1, 2, 4, 8, 16, 32, 64, 128};
    for (int i = 0; i < 8; ++i)
        if (a.prefs.grid == values[i])
            grid = i;
    if (ImGui::Combo("Grid", &grid, "1\0 2\0 4\0 8\0 16\0 32\0 64\0 128\0"))
        a.prefs.grid = values[grid];
    ImGui::SameLine();
    ImGui::Checkbox("Snap", &a.prefs.snap);
    ImGui::SameLine();
    if (ImGui::Button("Fit"))
        fitMap(a);
    ImGui::SameLine();
    if (ImGui::Button("Textures"))
        a.showTextures = !a.showTextures;
    ImGui::SameLine();
    if (ImGui::Button("Things"))
        a.showThings = !a.showThings;
    if (a.buildProcess) {
        ImGui::SameLine();
        ImGui::TextUnformatted("Building BSP...");
    }
}
} // namespace de
