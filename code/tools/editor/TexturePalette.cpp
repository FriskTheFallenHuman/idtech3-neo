#include "TexturePalette.h"
namespace de {
void drawTexturePalette(App &a) {
    drawTextureNames(a);
    if (a.selectedTexture >= 0 && a.selectedTexture < int(a.project.textures.size())) {
        auto &t = a.project.textures[a.selectedTexture];
        ImGui::Text("%s | %d x %d", t.name.c_str(), t.width, t.height);
        drawTextureView(a, t, 2.f * a.dpi);
        for (int i = 0; i < 3; ++i) {
            const char *labels[] = {"Apply upper", "Apply middle", "Apply lower"};
            if (i)
                ImGui::SameLine();
            if (ImGui::Button(labels[i])) {
                a.world.checkpoint();
                for (auto &l : a.world.lines)
                    if (l.selected > 0) {
                        auto &s = l.side[l.selected - 1];
                        if (i == 0)
                            s.toptexture = t.name;
                        else if (i == 1)
                            s.midtexture = t.name;
                        else
                            s.bottomtexture = t.name;
                    }
            }
        }
        if (ImGui::Button("Edit texture"))
            a.showTextureEdit = true;
    }
}
} // namespace de
