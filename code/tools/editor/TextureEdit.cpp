#include "TextureEdit.h"
namespace de {
void drawTextureEdit(App &a) {
    if (floatingPanel("Texture editor", &a.showTextureEdit, {720, 700})) {
        if (ImGui::Button("New texture")) {
            Texture t;
            t.name = "NEWTEX";
            a.project.textures.push_back(t);
            a.selectedTexture = int(a.project.textures.size() - 1);
            a.projectDirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Export texture WAD..."))
            a.action([&] {
                auto p = fileDialog(true, L"Doom texture WAD\0*.wad\0", L"wad");
                if (!p.empty())
                    a.project.exportTextures(p);
            });
        drawTextureNames(a);
        if (a.selectedTexture >= 0 && a.selectedTexture < int(a.project.textures.size())) {
            auto &t = a.project.textures[a.selectedTexture];
            ImGui::BeginDisabled(t.wadIndex == 2);
            bool change = inputName("Name", t.name);
            change |= ImGui::InputInt("Width", &t.width);
            change |= ImGui::InputInt("Height", &t.height);
            change |= ImGui::SliderInt("Texture set", &t.wadIndex, 0, 1);
            if (change) {
                a.projectDirty = true;
                a.clearImages();
            }
            drawTexturePatches(a, t);
            ImGui::EndDisabled();
            if (t.wadIndex == 2)
                ImGui::TextUnformatted("PNG material: edit the source PNG and rebuild its pack.");
            drawTextureView(a, t, 2 * a.dpi);
        }
    }
    ImGui::End();
}
} // namespace de
