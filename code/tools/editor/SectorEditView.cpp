#include "SectorEditView.h"
#include "TextureUVEditor.h"
namespace de {
bool editSector(SectorDef &e) {
    bool changed = false;
    changed |= ImGui::InputInt("Floor height", &e.floorheight);
    changed |= ImGui::InputInt("Ceiling height", &e.ceilingheight);
    changed |= inputName("Floor flat", e.floorflat);
    changed |= inputName("Ceiling flat", e.ceilingflat);
    float floorDegrees = e.rotation.floor / 1000.f, ceilingDegrees = e.rotation.ceiling / 1000.f;
    if (ImGui::InputFloat("Floor texture rotation", &floorDegrees, 0, 0, "%.3f")) {
        e.rotation.floor = textureMillidegrees(floorDegrees);
        changed = true;
    }
    if (ImGui::InputFloat("Ceiling texture rotation", &ceilingDegrees, 0, 0, "%.3f")) {
        e.rotation.ceiling = textureMillidegrees(ceilingDegrees);
        changed = true;
    }
    changed |= ImGui::SliderInt("Light", &e.lightlevel, 0, 255);
    if (ImGui::TreeNode("Floor UVs")) {
        changed |= editTextureUV(e.floorUV);
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Ceiling UVs")) {
        changed |= editTextureUV(e.ceilingUV);
        ImGui::TreePop();
    }
    changed |= ImGui::InputInt("Sector special", &e.special);
    changed |= ImGui::InputInt("Sector tag", &e.tag);
    return changed;
}
} // namespace de
