#include "TexturePatchView.h"
namespace de {
void drawTexturePatches(App &a, Texture &t) {
    bool changed = false;
    int erase = -1;
    for (size_t i = 0; i < t.patches.size(); ++i) {
        auto &p = t.patches[i];
        ImGui::PushID(int(i));
        changed |= inputName("Patch", p.name);
        int xy[] = {p.x, p.y};
        if (ImGui::InputInt2("Position", xy)) {
            p.x = xy[0];
            p.y = xy[1];
            changed = true;
        }
        if (ImGui::Button("Remove"))
            erase = int(i);
        ImGui::SameLine();
        if (ImGui::Button("Move up") && i > 0) {
            std::swap(t.patches[i], t.patches[i - 1]);
            changed = true;
        }
        ImGui::Separator();
        ImGui::PopID();
    }
    if (erase >= 0) {
        t.patches.erase(t.patches.begin() + erase);
        changed = true;
    }
    if (ImGui::BeginCombo("Add patch", "Choose a patch")) {
        for (auto &n : a.project.wad.patchNames())
            if (ImGui::Selectable(n.c_str())) {
                t.patches.push_back({0, 0, 1, 0, n});
                changed = true;
            }
        ImGui::EndCombo();
    }
    if (changed) {
        a.projectDirty = true;
        a.clearImages();
    }
}
} // namespace de
