#include "TexturePalView.h"
namespace de {
void drawTextureNames(App &a) {
    static ImGuiTextFilter filter;
    filter.Draw("Filter textures");
    ImGui::Checkbox("HD only", &a.hdWallsOnly);
    ImGui::BeginChild("Texture names", {0, 180 * a.dpi}, ImGuiChildFlags_Borders);
    for (size_t i = 0; i < a.project.textures.size(); ++i) {
        auto &t = a.project.textures[i];
        if (a.hdWallsOnly && t.wadIndex != 2)
            continue;
        if (!filter.PassFilter(t.name.c_str()))
            continue;
        ImGui::PushID(int(i));
        if (ImGui::Selectable(t.name.c_str(), a.selectedTexture == i))
            a.selectedTexture = int(i);
        ImGui::PopID();
    }
    ImGui::EndChild();
}
} // namespace de
