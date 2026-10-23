#include "ThingStripper.h"
namespace de {
void drawThingStripper(App &a) {
    if (floatingPanel("Thing stripper", &a.showStripper, {380, 200})) {
        static int type = 3004;
        ImGui::InputInt("Thing type", &type);
        int count = 0;
        for (auto &t : a.world.things)
            if (t.selected >= 0 && t.type == type)
                ++count;
        ImGui::Text("%d matching things in this map", count);
        if (ImGui::Button("Remove matching things")) {
            a.world.checkpoint();
            for (auto &t : a.world.things)
                if (t.selected >= 0 && t.type == type)
                    t.selected = -1;
            a.message("Removed " + std::to_string(count) + " things");
        }
    }
    ImGui::End();
}
} // namespace de
