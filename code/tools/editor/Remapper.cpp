#include "Remapper.h"
namespace de {
void drawRemapper(App &a) {
    if (floatingPanel("Remap map resources", &a.showRemapper, {420, 250})) {
        static int kind = 0, from = 0, to = 0;
        static std::string oldName, newName;
        ImGui::Combo("Kind", &kind, "Wall textures\0Flats\0Thing types\0Line specials\0");
        if (kind < 2) {
            inputName("From", oldName);
            inputName("To", newName);
        } else {
            ImGui::InputInt("From", &from);
            ImGui::InputInt("To", &to);
        }
        ImGui::TextWrapped(
            "Applies to every matching object in the current map. Undo is available.");
        if (ImGui::Button("Replace all")) {
            a.world.checkpoint();
            int n = kind == 0   ? remapTextures(a.world, oldName, newName)
                    : kind == 1 ? remapFlats(a.world, oldName, newName)
                    : kind == 2 ? remapThings(a.world, from, to)
                                : remapSpecials(a.world, from, to);
            a.message("Replaced " + std::to_string(n) + " values");
        }
    }
    ImGui::End();
}
} // namespace de
