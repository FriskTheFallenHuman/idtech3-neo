#include "TextureWindow.h"
namespace de {
void drawTextureWindow(App &a) {
    if (floatingPanel("Textures and flats", &a.showTextures, {520, 700})) {
        if (ImGui::BeginTabBar("Palettes")) {
            if (ImGui::BeginTabItem("Textures")) {
                drawTexturePalette(a);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Flats")) {
                drawFlats(a);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}
} // namespace de
